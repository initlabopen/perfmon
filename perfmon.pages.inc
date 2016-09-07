<?php

/**
 * @file perfmon.pages.inc
 */

/**
 * Page callback for run & review.
 */
function perfmon_page() {
  $tests = array();
  $output = array();
  // Retrieve the testlist.
  module_load_include('inc', 'perfmon');
  $testlist = perfmon_get_testlist();
  // Retrieve results from last run of the testlist.
  $tests = perfmon_get_stored_results();
  // Only users with the proper permission can run the testlist.
  if (user_access('run performance tests')) {
    $output += drupal_get_form('perfmon_run_form', $tests);
  }

  if (!empty($tests)) {
    // We have prior results, so display them.
    $output['results'] = perfmon_reviewed($testlist, $tests);
  }
  else {
    // If they haven't configured the site, prompt them to do so.
    drupal_set_message(t('It appears this is your first time using the performance testlist.'));
  }
  return $output;
}

function perfmon_reviewed($testlist, $tests) {
  $items = array();
  $last_run = variable_get('perfmon_last_run', '');
  $date = !empty($last_run) ? format_date($last_run) : '';
  $header = t('Review results from last run !date', array('!date' => $date));
  $desc = t("Here you can review the results from the last run of the testlist. You can run the testlist again by expanding the fieldset above.");

  foreach ($tests as $test) {
    // Skip this iteration if the result has no matching item in the testlist.
    if (!isset($testlist[$test['testname']])) {
      continue;
    }
    $message = $testlist[$test['testname']]['title'];
    $title = t('OK');
    $class = 'ok';
    $toggle = t('Skip');
    $token = drupal_get_token($test['testname']);
    $link_options = array(
      'query' => array('token' => $token),
    );

    $bold = FALSE;
    if ($test['testname'] == 'config') {
      $bold = TRUE;
    }


    $items[] = array(
      'title' => $title,
      'bold' => $bold,
      'value' => $test['result'],
      'class' => $class,
      'message' => $message,
      'help_link' => l(t('Details'), 'admin/reports/perfmon/help/' . $test['testname']),
    );
  }
  $output = theme('perfmon_reviewed', array('items' => $items, 'header' => $header, 'description' => $desc));
  // @todo #markup?
  return array('#markup' => $output);
}

function perfmon_mysql_page() {
  $mysqlstatus = array();
  $output = array();
  // Retrieve the testlist.
  module_load_include('inc', 'perfmon');
  $performance_variables = perfmon_get_mysql_performance_variables();

  if (!empty($performance_variables)) {
    // We have prior results, so display them.
    $output['results'] = perfmon_mysql_reviewed($performance_variables);
  }
  else {
    // If they haven't configured the site, prompt them to do so.
    drupal_set_message(t('Unable to get mysql global status'));
  }
  return $output;
}

function perfmon_mysql_reviewed($performance_variables) {
  $items = array();
  $header = t('MySQL Performance');
  $desc = t("Here you can review the results from checks mysql status and variables.");

  foreach ($performance_variables as $param_name => $param) {
    $message = $param['message'];
    $title = $param['title'];
    $class = $param['class'];
    $value = $param['display_value'];

    $bold = FALSE;
    // if ($test['testname'] == 'config') {
    //   $bold = TRUE;
    // }

    $items[] = array(
      'title' => $title,
      'bold' => $bold,
      'value' => $value,
      'class' => $class,
      'message' => $message,
    );
  }

  $output = theme('perfmon_mysql_reviewed', array('items' => $items, 'header' => $header, 'description' => $desc));

  return array('#markup' => $output);
}


function perfmon_run_form($form, &$form_state, $tests = NULL) {
  $form['run_form'] = array(
    '#type' => 'fieldset',
    '#title' => t('Run'),
    '#description' => t('Click the button below to run the performance testlist and review the results.'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($tests) ? FALSE : TRUE,
  );
  $form['run_form']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Run testlist'),
  );

  return $form;
}

function perfmon_run_form_submit($form, &$form_state) {
  module_load_include('inc', 'perfmon');
  $testlist = perfmon_get_testlist();

  // Use Batch to process the testlist.
  $batch = array('operations' => array(),
    'title' => t('Performance test'),
    'progress_message' => t('Progress @current out of @total.'),
    'error_message' => t('An error occurred. Rerun the process or consult the logs.'),
    'finished' => '_perfmon_batch_finished',
  );
  $log = variable_get('perfmon_log', TRUE);

  foreach ($testlist as $test_name => $test) {
    // Each test is its own operation. There could be a case where a single
    // test needs to run itself a batch operation, perhaps @todo?
    $batch['operations'][] = array('_perfmon_batch_op', array($test_name, $test, $log));
  }

  batch_set($batch);
  return;
}

function theme_perfmon_reviewed($variables) {
  // @todo
  //drupal_add_js(drupal_get_path('module', 'perfmon') . '/perfmon.js', array('scope' => 'footer'));
  $output = '<h3>' . $variables['header'] . '</h3>';
  $output .= '<p>' . $variables['description'] . '</p>';
  $output .= '<table class="system-status-report">';
  if (!empty($variables['items'])) {
    foreach ($variables['items'] as $item) {

      if ($item['bold']) {
        $output .= '<tr class="' . $item['class'] . '" style="font-size:medium; font-weight:bold;">';
      } else {
        $output .= '<tr class="' . $item['class'] . '">';
      }
      $output .= '<td class="status-icon"><div title="' . $item['title'] . '"><span class="element-invisible">' . $item['title'] . '</span></div></td>';
      $output .= '<td>' . $item['message'] . '</td>';
      $output .= '<td>' . $item['value'] . '</td>';
      $output .= '</tr>';
    }
  }
  $output .= '</table>';
  return $output;
}

function theme_perfmon_mysql_reviewed($variables) {
  $output = '<h3>' . $variables['header'] . '</h3>';
  $output .= '<p>' . $variables['description'] . '</p>';
  $output .= '<table class="system-status-report">';
  if (!empty($variables['items'])) {
    foreach ($variables['items'] as $item) {

      if ($item['bold']) {
        $output .= '<tr class="' . $item['class'] . '" style="font-size:medium; font-weight:bold;">';
      } else {
        $output .= '<tr class="' . $item['class'] . '">';
      }
      $output .= '<td>' . $item['title'] . '</td>';
      $output .= '<td>' . $item['value'] . '</td>';
      $output .= '<td>' . $item['message'] . '</td>';
      $output .= '</tr>';
    }
  }
  $output .= '</table>';
  return $output;
}
