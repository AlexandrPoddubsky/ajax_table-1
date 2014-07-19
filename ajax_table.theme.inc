<?php

define('AJAX_TABLE_THROBBER', 'none');

/**
 * Overrides theme_table
 */
function theme_ajax_table($elements) {
  $variables = $elements['elements'];
  
  $header = $variables['#header'];
  $attributes = $variables['#attributes'];
  $caption = $variables['#caption'];
  $colgroups = $variables['#colgroups'];
  $sticky = $variables['#sticky'];
  $empty = $variables['#empty'];
  
  $ajax_table_callback = $variables['#ajax_table_callback'];
  $ajax_table_wrapper = $variables['#ajax_table_wrapper'];
  $draggable = is_bool($variables['#draggable']) ? $variables['#draggable'] : FALSE;

  if ($variables['#draggable'] && is_array($variables['#draggable'])) {
    foreach ($variables['#draggable'] as $args) {
      if (is_array($args) && count($args) > 3) {
        if (is_null($args[0])) $args[0] = $attributes['id'];
        call_user_func_array('drupal_add_tabledrag', $args);
      }
      if (is_bool($args)) $draggable = $args;
    }
  }

  if ($variables['#draggable_groups'] &&
    is_array($variables['#draggable_groups'])
  ) {
    $groups = $variables['#draggable_groups'];
    foreach ($groups as $child => $parents) {
      if (!is_array($parents)) $groups[$child] = array($parents);
    }
    // allow multiple use on page
    $attached_groups = &drupal_static('ajax_table_groups', array());
    $attached_groups[] = array(
      'groups' => $groups,
      'id' => $attributes['id'],
    );
    drupal_add_js(array('ajax_table_groups' => $attached_groups), 'setting');
  }  

  drupal_add_js(drupal_get_path('module', 'ajax_table') .'/ajax_table.js');
  
  $rows = $variables['rows'];
  uasort($rows, 'element_sort');
  $rows_keys = element_children($rows);
  
  // Add sticky headers, if applicable.
  if (count($header) && $sticky) {
    drupal_add_js('misc/tableheader.js');
    // Add 'sticky-enabled' class to the table to identify it for JS.
    // This is needed to target tables constructed by this function.
    $attributes['class'][] = 'sticky-enabled';
  }
  
  $output = '<table' . drupal_attributes($attributes) . ">\n";

  if (isset($caption) && $caption) {
    $output .= '<caption>' . $caption . "</caption>\n";
  }

  // Format the table columns:
  if (count($colgroups)) {
    foreach ($colgroups as $number => $colgroup) {
      $attributes = array();

      // Check if we're dealing with a simple or complex column
      if (isset($colgroup['data'])) {
        foreach ($colgroup as $key => $value) {
          if ($key == 'data') {
            $cols = $value;
          }
          else {
            $attributes[$key] = $value;
          }
        }
      }
      else {
        $cols = $colgroup;
      }

      // Build colgroup
      if (is_array($cols) && count($cols)) {
        $output .= ' <colgroup' . drupal_attributes($attributes) . '>';
        $i = 0;
        foreach ($cols as $col) {
          $output .= ' <col' . drupal_attributes($col) . ' />';
        }
        $output .= " </colgroup>\n";
      }
      else {
        $output .= ' <colgroup' . drupal_attributes($attributes) . " />\n";
      }
    }
  }

  // Add the 'empty' row message if available.
  if (!count($rows_keys) && $empty) {
    $header_count = 0;
    foreach ($header as $header_cell) {
      if (is_array($header_cell)) {
        $header_count += isset($header_cell['colspan']) ? $header_cell['colspan'] : 1;
      }
      else {
        $header_count++;
      }
    }
    $rows[] = array(array('data' => $empty, 'colspan' => $header_count, 'class' => array('empty', 'message')));
    $rows_keys = element_children($rows);
  }

  // Format the table header:
  if (count($header)) {
    $ts = tablesort_init($header);
    // HTML requires that the thead tag has tr tags in it followed by tbody
    // tags. Using ternary operator to check and see if we have any rows.
    $output .= (count($rows_keys) ? ' <thead><tr>' : ' <tr>');
    foreach ($header as $cell) {   
      $cell = ajax_tablesort_header($cell, $header, $ts, $ajax_table_callback);  
      $output .= _theme_table_cell($cell, TRUE);
    }
    // Using ternary operator to close the tags based on whether or not there are rows
    $output .= (count($rows_keys) ? " </tr></thead>\n" : "</tr>\n");
  }
  else {
    $ts = array();
  }

  // Format the table rows:
  if (count($rows_keys)) {
    $output .= "<tbody>\n";
    $flip = array('even' => 'odd', 'odd' => 'even');
    $class = 'even';
    foreach ($rows_keys as $number) {

      $row = $rows[$number];
      $attributes = array();
      if (isset($row['#attributes'])) $attributes = $row['#attributes'];

      // Check if we're dealing with a simple or complex row
      if (isset($row['data'])) $cells = $row['data'];
      else $cells = $row;
      
      $cells_keys = element_children($cells);
      if (count($cells_keys)) {

        // Add odd/even class
        if (empty($row['no_striping'])) {
          $class = $flip[$class];
          $attributes['class'][] = $class;
        }
        
        // Add draggable
        if ($draggable) {
          $attributes['class'][] = 'draggable';
        }

        // Build row
        $output .= ' <tr' . drupal_attributes($attributes) . '>';
        $i = 0;
        foreach ($cells_keys as $cell) {
          $cell = $cells[$cell];
          if (!isset($cell['data'])) $cell = array('data' => $cell);
          if (is_array($cell) && isset($cell['#attributes']) &&
            is_array($cell['#attributes'])
          ) $cell = array_merge_recursive($cell, $cell['#attributes']);
          if (is_array($cell)) ajax_table_strip_properties($cell);
          $cell = tablesort_cell($cell, $header, $ts, $i++);
          $output .= _theme_table_cell($cell);
        }
        $output .= " </tr>\n";
      }
    }
    $output .= "</tbody>\n";
  }

  $output .= "</table>\n";
  
  $attributes = array(
    'id' => $ajax_table_wrapper,
    'class' => array('ajax-table-table-wrapper'),
  );
  $output = '<div' . drupal_attributes($attributes) . '>' . $output . '</div>';
  unset($variables['rows']);
  return $output . drupal_render_children($variables);
}

/**
 * Overrides tablesort_header
 */
function ajax_tablesort_header($cell, $header, $ts, $ajax_table_callback = '') {
  // Special formatting for the currently sorted column header.
  if (is_array($cell) && isset($cell['field'])) {
    $title = t('sort by @s', array('@s' => $cell['data']));
    if ($cell['data'] == $ts['name']) {
      $ts['sort'] = (($ts['sort'] == 'asc') ? 'desc' : 'asc');
      $cell['class'][] = 'active';
      $image = theme('tablesort_indicator', array('style' => $ts['sort']));
    }
    else {
      // If the user clicks a different header, we want to sort ascending initially.
      $ts['sort'] = 'asc';
      $image = '';
    }
    $query = array_merge($ts['query'],
      array('sort' => $ts['sort'], 'order' => $cell['data']));    
    $ajax_table_link = array(
      '#type' => 'link',
      '#title' => $cell['data'] . $image,
      '#href' => $ajax_table_callback,
      '#options' => array(
        'attributes' => array(
          'class' => array(
            'use-ajax',
            'ajax-table-link',
            'ajax-table-ajax-link',
          ),
          'style' => 'display: none',
          'title' => $title,
        ),
        'query' => $query,
        'html' => TRUE,
        ),
      '#ajax' => array(
        'progress' => array('type' => AJAX_TABLE_THROBBER),
      ),
    );
    $non_js_link = l($cell['data'] . $image, $_GET['q'], array(
      'attributes' => array(
        'class' => array(
          'ajax-table-link',
          'ajax-table-non-js-link',
        ),      
        'title' => $title
      ),
      'query' => $query,
      'html' => TRUE)
    );
    $cell['data'] = drupal_render($ajax_table_link) . $non_js_link;
    
    unset($cell['field'], $cell['sort']);
  }
  return $cell;
}

/**
 * Overrides theme_pager
 */
function theme_ajax_pager($elements) {
  $variables = $elements['elements'];
  $tags = $variables['#tags'];
  $element = $variables['#element'];
  $parameters = $variables['#parameters'];
  $quantity = $variables['#quantity'];  
  $ajax_table_callback = $variables['#ajax_table_callback'];
  $ajax_table_wrapper = $variables['#ajax_table_wrapper'];

  global $pager_page_array, $pager_total;
  if (!isset($pager_page_array[$element]) ||
    !isset($pager_total[$element])
  ) return '';

  // Calculate various markers within this pager piece:
  // Middle is used to "center" pages around the current page.
  $pager_middle = ceil($quantity / 2);
  // current is the page we are currently paged to
  $pager_current = $pager_page_array[$element] + 1;  
  // first is the first page listed by this pager piece (re quantity)
  $pager_first = $pager_current - $pager_middle + 1;
  // last is the last page listed by this pager piece (re quantity)
  $pager_last = $pager_current + $quantity - $pager_middle;
  // max is the maximum page number
  $pager_max = $pager_total[$element];
  // End of marker calculations.

  // Prepare for generation loop.
  $i = $pager_first;
  if ($pager_last > $pager_max) {
    // Adjust "center" if at end of query.
    $i = $i + ($pager_max - $pager_last);
    $pager_last = $pager_max;
  }
  if ($i <= 0) {
    // Adjust "center" if at start of query.
    $pager_last = $pager_last + (1 - $i);
    $i = 1;
  }
  // End of generation loop preparation.

  $li_first = theme('ajax_pager_first', array('text' => (isset($tags[0]) ? $tags[0] : t('« first')), 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
  $li_previous = theme('ajax_pager_previous', array('text' => (isset($tags[1]) ? $tags[1] : t('‹ previous')), 'element' => $element, 'interval' => 1, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
  $li_next = theme('ajax_pager_next', array('text' => (isset($tags[3]) ? $tags[3] : t('next ›')), 'element' => $element, 'interval' => 1, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
  $li_last = theme('ajax_pager_last', array('text' => (isset($tags[4]) ? $tags[4] : t('last »')), 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));

  if ($pager_total[$element] > 1) {
    if ($li_first) {
      $items[] = array(
        'class' => array('pager-first'),
        'data' => $li_first,
      );
    }
    if ($li_previous) {
      $items[] = array(
        'class' => array('pager-previous'),
        'data' => $li_previous,
      );
    }

    // When there is more than one page, create the pager list.
    if ($i != $pager_max) {
      if ($i > 1) {
        $items[] = array(
          'class' => array('pager-ellipsis'),
          'data' => '…',
        );
      }
      // Now generate the actual pager piece.
      for (; $i <= $pager_last && $i <= $pager_max; $i++) {
        if ($i < $pager_current) {
          $items[] = array(
            'class' => array('pager-item'),       
            'data' => theme('ajax_pager_previous', array('text' => $i, 'element' => $element, 'interval' => ($pager_current - $i), 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper)),
          );
        }
        if ($i == $pager_current) {
          $items[] = array(
            'class' => array('pager-current'),
            'data' => $i,
          );
        }
        if ($i > $pager_current) {
          $items[] = array(
            'class' => array('pager-item'),         
            'data' => theme('ajax_pager_next', array('text' => $i, 'element' => $element, 'interval' => ($i - $pager_current), 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper)),
          );
        }
      }
      if ($i < $pager_max) {
        $items[] = array(
          'class' => array('pager-ellipsis'),
          'data' => '…',
        );
      }
    }
    // End generation.
    if ($li_next) {
      $items[] = array(
        'class' => array('pager-next'),
        'data' => $li_next,
      );
    }
    if ($li_last) {
      $items[] = array(
        'class' => array('pager-last'),
        'data' => $li_last,
      );
    }
    $output = '<h2 class="element-invisible">' . t('Pages') . '</h2>' . theme('item_list', array(
      'items' => $items,
      'attributes' => array('class' => array('pager')),
    ));
    $attributes = array(
      'id' => $ajax_table_wrapper,
      'class' => 'ajax-table-pager-wrapper',
    );
    $output = '<div' . drupal_attributes($attributes) . '>' . $output . '</div>';
    return $output;
  }
}

/**
 * Overrides theme_pager_first
 */
function theme_ajax_pager_first($variables) {
  $text = $variables['text'];
  $element = $variables['element'];
  $parameters = $variables['parameters'];
  $ajax_table_callback = $variables['ajax_table_callback'];
  $ajax_table_wrapper = $variables['ajax_table_wrapper'];  
  
  global $pager_page_array;
  $output = '';

  // If we are anywhere but the first page
  if ($pager_page_array[$element] > 0) {
    $output = theme('ajax_pager_link', array('text' => $text, 'page_new' => pager_load_array(0, $element, $pager_page_array), 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));    
  }

  return $output;
}

/**
 * Overrides theme_pager_previous
 */
function theme_ajax_pager_previous($variables) {
  $text = $variables['text'];
  $element = $variables['element'];
  $interval = $variables['interval'];
  $parameters = $variables['parameters'];
  $ajax_table_callback = $variables['ajax_table_callback'];
  $ajax_table_wrapper = $variables['ajax_table_wrapper'];  
  
  global $pager_page_array;
  $output = '';

  // If we are anywhere but the first page
  if ($pager_page_array[$element] > 0) {
    $page_new = pager_load_array($pager_page_array[$element] - $interval, $element, $pager_page_array);

    // If the previous page is the first page, mark the link as such.
    if ($page_new[$element] == 0) {
      $output = theme('ajax_pager_first', array('text' => $text, 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));      
    }
    // The previous page is not the first page.
    else {
      $output = theme('ajax_pager_link', array('text' => $text, 'page_new' => $page_new, 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
    }
  }

  return $output;
}

/**
 * Overrides theme_pager_next
 */
function theme_ajax_pager_next($variables) {
  $text = $variables['text'];
  $element = $variables['element'];
  $interval = $variables['interval'];
  $parameters = $variables['parameters'];
  $ajax_table_callback = $variables['ajax_table_callback'];
  $ajax_table_wrapper = $variables['ajax_table_wrapper'];  
  
  global $pager_page_array, $pager_total;
  $output = '';

  // If we are anywhere but the last page
  if ($pager_page_array[$element] < ($pager_total[$element] - 1)) {
    $page_new = pager_load_array($pager_page_array[$element] + $interval, $element, $pager_page_array);
    // If the next page is the last page, mark the link as such.
    if ($page_new[$element] == ($pager_total[$element] - 1)) {     
      $output = theme('ajax_pager_last', array('text' => $text, 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
    }
    // The next page is not the last page.
    else {  
      $output = theme('ajax_pager_link', array('text' => $text, 'page_new' => $page_new, 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));
    }
  }

  return $output;
}

/**
 * Overrides theme_pager_last
 */
function theme_ajax_pager_last($variables) {
  $text = $variables['text'];
  $element = $variables['element'];
  $parameters = $variables['parameters'];
  $ajax_table_callback = $variables['ajax_table_callback'];
  $ajax_table_wrapper = $variables['ajax_table_wrapper'];  
  
  global $pager_page_array, $pager_total;
  $output = '';

  // If we are anywhere but the last page
  if ($pager_page_array[$element] < ($pager_total[$element] - 1)) {
    $output = theme('ajax_pager_link', array('text' => $text, 'page_new' => pager_load_array($pager_total[$element] - 1, $element, $pager_page_array), 'element' => $element, 'parameters' => $parameters, 'ajax_table_callback' => $ajax_table_callback, 'ajax_table_wrapper' => $ajax_table_wrapper));    
  }

  return $output;
}

/**
 * Overrides theme_pager_link
 */
function theme_ajax_pager_link($variables) {
  $text = $variables['text'];
  $page_new = $variables['page_new'];
  $element = $variables['element'];
  $parameters = $variables['parameters'];
  $attributes = $variables['attributes'];
  $ajax_table_callback = $variables['ajax_table_callback'];
  $ajax_table_wrapper = $variables['ajax_table_wrapper']; 

  $page = isset($_GET['page']) ? $_GET['page'] : '';
  if ($new_page = implode(',', pager_load_array($page_new[$element], $element, explode(',', $page)))) {
    $parameters['page'] = $new_page;
  }

  $query = array();
  if (count($parameters)) {
    $query = drupal_get_query_parameters($parameters, array());
  }
  if ($query_pager = pager_get_query_parameters()) {
    $query = array_merge($query, $query_pager);
  }

  // Set each pager link title
  if (!isset($attributes['title'])) {
    static $titles = NULL;
    if (!isset($titles)) {
      $titles = array(
        t('« first') => t('Go to first page'),
        t('‹ previous') => t('Go to previous page'),
        t('next ›') => t('Go to next page'),
        t('last »') => t('Go to last page'),
      );
    }
    if (isset($titles[$text])) {
      $attributes['title'] = $titles[$text];
    }
    elseif (is_numeric($text)) {
      $attributes['title'] = t('Go to page @number', array('@number' => $text));
    }
  }

  // @todo l() cannot be used here, since it adds an 'active' class based on the
  //   path only (which is always the current path for pager links). Apparently,
  //   none of the pager links is active at any time - but it should still be
  //   possible to use l() here.
  // @see http://drupal.org/node/1410574
  $ajax_table_link = array(
    '#type' => 'link',
    '#title' => check_plain($text),
    '#href' => $ajax_table_callback,
    '#options' => array(
      'attributes' => array_merge_recursive(
        array(
          'class' => array(
            'use-ajax',
            'ajax-table-link',
            'ajax-table-ajax-link',
          ),
          'style' => 'display: none',
        ),
        $attributes
      ),
      'query' => $query,
      'html' => TRUE,
      ),
    '#ajax' => array(
      'progress' => array('type' => AJAX_TABLE_THROBBER),
    ),
  );
  $attributes['href'] = url($_GET['q'], array('query' => $query));
  $attributes['class'][] = 'ajax-table-link';
  $attributes['class'][] = 'ajax-table-non-js-link';  
  $non_js_link = '<a' . drupal_attributes($attributes) . '>' .
    check_plain($text) . '</a>';
  return drupal_render($ajax_table_link) . $non_js_link;
}