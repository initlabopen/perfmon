<?php

/**
 * @file
 * Stand-alone perfmon test system.
 */

// Define the version of this file in case it's used outside of its module.
define('perfmon_VERSION', '7.x-1.1');

/**
 * Public function for running Perfmon tests and returning results.
 *
 * @param array $testlist Array of tests to run, indexed by testname.
 * @param boolean $log Whether to log test processing using perfmon_log.
 * @return array Results from running checklist, indexed by module namespace.
 */
function perfmon_run($testlist = NULL, $log = FALSE) {
  if (!$testlist) {
    $testlist = _perfmon_performance_tests();
  }
  $results = _perfmon_run($testlist, $log);

  return $results;
}

/**
 * Private function the review and returns the full results.
 */
function _perfmon_run($testlist, $log = FALSE) {
  $results = array();

  foreach ($testlist as $test_name => $arguments) {
    $test_result = _perfmon_run_test($test_name, $arguments, $log);
    if (!empty($test_result)) {
      $results[$test_name] = $test_result;
    }
  }
  return $results;
}

/**
 * Run a single Security Review check.
 */
function _perfmon_run_test($test_name, $test, $log, $store = FALSE) {
  $last_test = array();
  if ($store) {
    // Get the results of the last check.
    $last_test = perfmon_get_last_test($test_name);
  }
  $test_result = array();
  $return = array('result' => NULL);
  $function = $test['callback'];
  if (function_exists($function)) {
    $return = call_user_func($function, $last_test);
  }
  $test_result = array_merge($test, $return);
  $test_result['lastrun'] = REQUEST_TIME;

  if ($log && !is_null($return['result'])) { // Do not log if result is NULL.
    $variables = array('!name' => $test_result['title']);
    _perfmon_log($test_name, '!name tested', $variables, WATCHDOG_INFO);
  }
  return $test_result;
}

/**
 * Core Perfmon checks.
 */
function _perfmon_performance_tests() {
  $tests = array();
  $tests['config'] = array(
    'bold' => TRUE,
    'title' => t('Performance score'),
    'callback' => 'perfmon_test_server_configuration',
    'description' => t('Drupal installation files and directories (except required) are not writable by the server.'),
  );
  $tests['cpu'] = array(
    'title' => t('CPU (ops per second)'),
    'callback' => 'perfmon_test_cpu',
    'description' => t('Untrusted users are not allowed to input dangerous HTML tags.'),
  );
  $tests['files'] = array(
    'title' => t('Files operations (ops per second)'),
    'callback' => 'perfmon_test_filesop',
    'description' => t('Dangerous tags were not found in any submitted content (fields).'),
  );
  $tests['db_read'] = array(
    'title' => t('DB read operations (ops per second)'),
    'callback' => 'perfmon_test_db_read',
    'description' => t('Error reporting set to log only.'),
  );
  $tests['db_write'] = array(
    'title' => t('DB write operations (ops per second)'),
    'callback' => 'perfmon_test_db_write',
    'description' => t('Private files directory is outside the web server root.'),
  );
  $tests['db_update'] = array(
    'title' => t('DB update operations (ops per second)'),
    'callback' => 'perfmon_test_db_update',
    'description' => t('Only safe extensions are allowed for uploaded files and images.'),
  );

  return $tests;
}

/**
 * Test configuration
 */
function perfmon_test_server_configuration() {
  global $_SESSION;

  $domain = $_SERVER['SERVER_NAME'];
  $port = $_SERVER['SERVER_PORT'];
  $request_uri = '/'.drupal_get_path('module', 'perfmon').'/perfmon.test.php';

  $url = 'http://' . $domain . $request_uri;

  $execcount = 5;
  $exectime = 0;
  $count = 10;

  $curl_arr = array();
  $master = curl_multi_init();

  $cookiestr = "";

  foreach ($_COOKIE as $key => $value) {
    $cookiestr = $cookiestr . $key . "=" . $value . "; ";
  }

  for ($k = 0; $k < $execcount; $k++) {

    for($i = 0; $i < $count; $i++)
    {
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 15);
        curl_setopt($ch, CURLOPT_COOKIE, $cookiestr);
        curl_setopt($ch, CURLOPT_PORT, $port);
        curl_setopt($ch, CURLOPT_MAXREDIRS, 0);
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, 1);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        $curl_arr[$i] = $ch;

        curl_multi_add_handle($master, $curl_arr[$i]);
    }

    do {
        curl_multi_exec($master,$running);
    } while($running > 0);

    for ($i=0; $i < $count; $i++) {

      $info = curl_getinfo($curl_arr[$i]);

      $exectime += $info['starttransfer_time'] - $info['pretransfer_time'];

      curl_multi_remove_handle($master, $curl_arr[$i]);
      curl_close($curl_arr[$i]);
    }

    sleep(2);
  }

  return array('result' => round(1 / ( $exectime / $execcount / $count ), 0), 'value' => '0');
}

/**
 * Test CPU.
 */
function perfmon_test_cpu() {

  $exectime = 0;
  $count = 100000;

  for ($i = 0; $i < $count; $i++) {
    $starttime = microtime(TRUE);

    $test = 0;

    $test++;
    $test--;
    $test = 2 / 5;
    $test = 2 * 5;

    $endtime = microtime(TRUE);

    $exectime += $endtime - $starttime;

  }

  return array('result' => round( 1 / ($exectime / $count) , 0 ), 'value' => '0');
}

/**
 * Test file operations.
 */
function perfmon_test_filesop() {
  $tempdir = file_directory_temp();

  $count = 1000;
  $exectime = microtime(TRUE);

  for ($i = 0; $i < $count; $i++) {
    $filecont = "<?php phpinfo(); print(''" . $i . "'')";
    $filename = $tempdir . "/phptest" . $i . ".php";
    $file = fopen( $filename, "w" );
    fwrite($file, $filecont);
    unlink($filename);
  }
  $exectime = microtime(TRUE) - $exectime;

  return array('result' => round( 1 / ($exectime / $count) , 0), 'value' => '0');
}

function perfmon_test_db_prepare($testname, $count) {

  for ($i=0; $i < $count; $i++) {
    db_insert('perfmon_test')
      ->fields(array(
        'id' => $i,
        'testname' => 'dbtest',
        'data' => 'aaaaaaaaaaaaaaaaaaaaa',
      ))
      ->execute();
  }

}

function perfmon_test_db_read() {
  $exectime = 0;
  $count = 1000;

  db_truncate('perfmon_test')->execute();

  perfmon_test_db_prepare('test_db_read', $count);

  $exectime = microtime(TRUE);

  for ($i = 0; $i < $count; $i++) {
    $query = db_select('perfmon_test', 'pt');
    $query->fields('pt', array('id', 'testname', 'data'));
    $query->condition('pt.id', $i, '=');
    $result = $query->execute();
  }

  $exectime = microtime(TRUE) - $exectime;

  return array('result' => round( 1 / ($exectime / $count) , 0), 'value' => '0');
}

function perfmon_test_db_write() {
  $exectime = 0;
  $count = 1000;

  db_truncate('perfmon_test')->execute();

  $exectime = microtime(TRUE);

  perfmon_test_db_prepare('test_db_write', $count);

  $exectime = microtime(TRUE) - $exectime;

  return array('result' => round( 1 / ($exectime / $count) , 0), 'value' => '0');
}

function perfmon_test_db_update() {
  $exectime = 0;
  $count = 1000;

  db_truncate('perfmon_test')->execute();

  perfmon_test_db_prepare('test_db_update', $count);

  $exectime = microtime(TRUE);

  for ($i = 0; $i < $count; $i++) {
    $query = db_update('perfmon_test');
    $query->fields(array(
      'data' => 'bbbbbbbbbbbbbbbbbbbbb',
    ));
    $query->condition('id', $i);
    $num_updated = $query->execute();
  }

  $exectime = microtime(TRUE) - $exectime;

  return array('result' => round( 1 / ($exectime / $count) , 0), 'value' => '0');
}

/**
 * Get core performance tests.
 *
 * @return array
 *   Array of checks indexed by test name. e.g.:
 *   'perfmon' => array(
 *     'test_name' => array(...)
 *   )
 */
function perfmon_get_testlist() {
  $tests = _perfmon_performance_tests();

  return $tests;
}

function perfmon_get_mysqlvariables() {
  $mysqlvariables = db_query("SHOW GLOBAL VARIABLES")->fetchAll();
  $result = perfmon_convert2array($mysqlvariables);

  return $result;
}

function perfmon_get_mysqlstatus() {
  $mysqlstatus = db_query("SHOW GLOBAL STATUS")->fetchAll();
  $result = perfmon_convert2array($mysqlstatus);

  return $result;
}

function perfmon_convert2array($variables) {
  $result = array();

  foreach ($variables as $parameter) {
    $result[$parameter->Variable_name] = $parameter->Value;
  }

  return $result;
}

function secondsToTime($seconds) {
    $dtF = new \DateTime('@0');
    $dtT = new \DateTime("@$seconds");
    return $dtF->diff($dtT)->format(t('%a days, %h hours, %i minutes and %s seconds'));
}

function formatBytes($bytes, $precision = 2) {
    $units = array('B', 'KB', 'MB', 'GB', 'TB');

    $bytes = max($bytes, 0);
    $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
    $pow = min($pow, count($units) - 1);

    // Uncomment one of the following alternatives
    $bytes /= pow(1024, $pow);
    // $bytes /= (1 << (10 * $pow));

    return round($bytes, $precision) . ' ' . $units[$pow];
}

function perfmon_get_mysql_performance_variables() {
  $variables = perfmon_get_mysqlvariables();
  $status = perfmon_get_mysqlstatus();

  $performance_variables = array();


  $uptime = $status['Uptime'];
  $performance_variables['uptime'] = array(
    'value' => $uptime,
    'title' => t('MySQL Uptime'),
    'display_value' => secondsToTime($uptime),
    'message' => t('MySQL Server Uptime'),
    'class' => 'ok'
  );

  if ($uptime < 86400) {
    $performance_variables['uptime']['message'] = t('MySQL started within last 24 hours - recommendations may be inaccurate');
    $performance_variables['uptime']['class'] = 'error';
  }

  $memory_global = $variables['key_buffer_size'] + $variables['tmp_table_size'] + $variables['innodb_buffer_pool_size'] +
                   $variables['query_cache_size'] + $variables['innodb_additional_mem_pool_size'] + $variables['innodb_log_buffer_size'];
  $performance_variables['memory_global'] = array(
    'value' => $memory_global,
    'title' => t('Global Memory'),
    'display_value' => formatBytes($memory_global),
    'message' => t('Global memory size (key_buffer_size + tmp_table_size + innodb_buffer_pool_size + innodb_additional_mem_pool_size + innodb_log_buffer_size + query_cache_size)'),
    'class' => 'ok'
  );

  $memory_per_connection = $variables['read_buffer_size'] + $variables['read_rnd_buffer_size'] +
                           $variables['sort_buffer_size'] + $variables['join_buffer_size'];
  $performance_variables['memory_per_connection'] = array(
    'value' => $memory_per_connection,
    'title' => t('Memory per connection'),
    'display_value' => formatBytes($memory_per_connection),
    'message' => t('Memory per connection (read_buffer_size + read_rnd_buffer_size + sort_buffer_size + thread_stack + join_buffer_size)'),
    'class' => 'ok'
  );

  $performance_variables['max_connections'] = array(
    'value' => $variables['max_connections'],
    'title' => 'Max connections',
    'display_value' => $variables['max_connections'],
    'message' => t('Max user connections ( max_connections )'),
    'class' => 'ok'
  );

  $max_memory = $performance_variables['memory_global']['value'] +
                $performance_variables['max_connections']['value'] * $performance_variables['memory_per_connection']['value'];
  $performance_variables['max_memory'] = array(
    'value' => $max_memory,
    'title' => 'Max. Memory',
    'display_value' => formatBytes($max_memory),
    'message' => t('Max Mysql memory (Global memory + Memory per connection * Max connections). Check that this variable lower than 85% of server RAM.'),
    'class' => 'ok'
  );

  $performance_variables['query_cache_size'] = array(
    'value' => $variables['query_cache_size'],
    'title' => t('Query cache size'),
    'display_value' => formatBytes($variables['query_cache_size']),
    'message' => t('Query cache size ( query_cache_size ). Recommended 128M'),
    'class' => 'ok',
  );

  if ($variables['query_cache_size'] != (1024 * 1024 * 128)) {
    $performance_variables['query_cache_size']['message'] = t( 'Query cache size query_cache_size. Recommended 128M. Increasing the query_cache size over 128M may reduce performance.' );
    $performance_variables['query_cache_size']['class'] = 'error';
  }

  $query_cache_efficiency = $status['Qcache_hits'] / ($status['Com_select'] + $status['Qcache_hits']);
  $performance_variables['query_cache_efficiency'] = array(
    'value' => $query_cache_efficiency,
    'title' => t('Query cache efficiency'),
    'display_value' => round($query_cache_efficiency * 100, 2) . '%',
    'message' => t('Query cache efficiency'),
    'class' => 'ok'
  );

  if ($query_cache_efficiency < 0.2) {
    $performance_variables['query_cache_efficiency']['message'] = t('Query cache efficiency. This parameter is low try to increase query_cache_limit.');
    $performance_variables['query_cache_efficiency']['class'] = 'error';
  }

  $query_cache_prunes_per_day = $status['Qcache_lowmem_prunes'] / ( $status['Uptime'] / 86400 );
  $performance_variables['query_cache_prunes_per_day'] = array(
    'value' => $query_cache_prunes_per_day,
    'title' => t('Query cache prunes per day'),
    'display_value' => round($query_cache_prunes_per_day, 2),
    'message' => t('Query cache prunes per day. If value grows rapidly you need to increase query_cache_size.'),
    'class' => 'ok',
  );

  $total_sorts = $status['Sort_scan'] + $status['Sort_range'];
  $performance_variables['total_sorts'] = array(
    'value' => $total_sorts,
    'title' => t('Total sorts'),
    'display_value' => $total_sorts,
    'message' => t('Total sorts count.'),
    'class' => 'ok',
  );

  $total_sorts_temp = $status['Sort_merge_passes'] / $performance_variables['total_sorts']['value'];
  $performance_variables['total_sorts_temp'] = array(
    'value' => $total_sorts_temp,
    'title' => t('Total sorts using disk'),
    'display_value' => round($total_sorts_temp * 100, 2) . '%',
    'message' => t('Sorts requiring temporary tables. If value grater than 10% then you need to increase sort_buffer_size and read_rnd_buffer_size.'),
    'class' => 'ok',
  );

  if ($total_sorts_temp > 0.1) {
    $performance_variables['total_sorts_temp']['class'] = 'error';
  }


  $created_tmp_tables_dsk = $status['Created_tmp_disk_tables'] / $status['Created_tmp_tables'];
  $performance_variables['created_tmp_tables_dsk'] = array(
    'value' => $created_tmp_tables_dsk,
    'title' => t('Temporary tables created on disk'),
    'display_value' => round($created_tmp_tables_dsk * 100, 2) . '%',
    'message' => t('Temporary tables created on disk. If value grater than 30% then you need to increase tmp_table_size and tmp_table_size. When making adjustments, make tmp_table_size/max_heap_table_size equal.'),
    'class' => 'ok',
  );

  if ($created_tmp_tables_dsk > 0.3) {
    $performance_variables['created_tmp_tables_dsk']['class'] = 'error';
  }

  $open_files = $status['Open_files'] / $variables['open_files_limit'];
  $performance_variables['open_files'] = array(
    'value' => $open_files,
    'title' => t('Open files'),
    'display_value' => round($open_files * 100, 2) . '%',
    'message' => t('Open file limit used. If value grater than 85% then you need to increase open_files_limit.'),
    'class' => 'ok',
  );

  if ($open_files > 0.85) {
    $performance_variables['open_files']['class'] = 'error';
  }

  $innodb_buffer_efficiency = 1 - $status['Innodb_buffer_pool_reads'] / $status['Innodb_buffer_pool_read_requests'];
  $performance_variables['innodb_buffer_efficiency'] = array(
    'value' => $innodb_buffer_efficiency,
    'title' => t('Innodb buffer pool efficiency'),
    'display_value' => round($innodb_buffer_efficiency * 100, 2) . '%',
    'message' => t('InnoDB Buffer pool read cache effiency. If value lower than 95% then you need to increase innodb_buffer_pool_size.'),
    'class' => 'ok',
  );

  if ($innodb_buffer_efficiency < 0.95) {
    $performance_variables['innodb_buffer_efficiency']['class'] = 'error';
  }

  $performance_variables['innodb_flush_log_at_trx_commit'] = array(
    'value' => $variables['innodb_flush_log_at_trx_commit'],
    'title' => t('innodb_flush_log_at_trx_commit'),
    'display_value' => $variables['innodb_flush_log_at_trx_commit'],
    'message' => t('innodb_flush_log_at_trx_commit. Recommended value is 2.'),
    'class' => 'ok',
  );

  if ($variables['innodb_flush_log_at_trx_commit'] != 2) {
    $performance_variables['innodb_flush_log_at_trx_commit']['class'] = 'error';
  }

  return $performance_variables;
}

function _perfmon_log($module, $check_name, $message, $variables, $type) {
  module_invoke_all('perfmon_log', $module, $check_name, $message, $variables, $type);
}
