<?php

/**
 * @file
 * Contains methods to search solr and display results.  depends on Apache_Solr_Php client.
 */

/**
 * Islandora Solr Results
 */
class IslandoraSolrResults {

  static $facetSeparator = '~'; //used to separate facets in url
  static $slashReplacement = '~slsh~'; // a pattern used to replace / in url's the slash breaks drupal clean url's
  public static $SEARCH_CLASS_ADVANCED_SEARCH_NUMBER_FIELDS = 5;
  public $facetFieldArray = array();
  public $searchFieldArray = array();
  public $resultFieldArray = array();
  public $allSubsArray = array();

  /**
   * Constructor
   */
  function IslandoraSolrResults() {
    module_load_include('php', 'islandora_solr_search', 'SolrPhpClient/Apache/Solr/Service');
    module_load_include('inc', 'islandora_solr_search', 'includes/common');
    $this->prepFieldSubstitutions();
  }

  /**
   * Output the main body of the search results
   *
   * @param type $solrQueryProcessor
   * @param type $title
   * @param type $output
   * @return string 
   */
  function displayResults($solrQueryProcessor, $title = "Search Results", $output = '') {

    $apacheSolrResult = $solrQueryProcessor->solrResult;
    $total = (int) $apacheSolrResult->response->numFound;
    $end = min(($solrQueryProcessor->solrLimit + $solrQueryProcessor->solrStart), $total);

    // Check for empty resultset
    if ($total === 0) {
      $output = "<h2>Search Results</h2>";
      $output .= "<div>Sorry, but your search returned no results.</div>";
      return $output;
    }

    // Initialize drupal-style pager
    islandora_solr_search_pager_init($total, $solrQueryProcessor->solrLimit);

    // Get secondary display profiles
    $secondary_display_profiles = module_invoke_all('islandora_solr_secondary_display');
    // How about this for an expected return?
    // return array(
    // 'machine-name' = array(
    //   'name' => 'Human Readable Name',
    //   'module' => 'module_name',
    //   'file' => 'FileName.inc',
    //   'class' => 'ClassName',
    //   'function' => 'function_name',
    //   'description' => 'A description of the display profile',
    //   'logo' => 'Some text, some <html>, an <img>, etc used to link to this output,
    // );
    // check if
    // if the islandora_solr_search admin form has never been saved, then
    // the variable 'islandora_solr_secondary_display' won't be set yet.
    $secondary_array = variable_get('islandora_solr_secondary_display', array());

    // count number of elements in array
    $array_count = count($secondary_array);
    // count number of disabled displays
    $i = 0;
    foreach ($secondary_array as $key => $value) {
      if ($value === 0) {
        $i++;
      }
    }

    // if number of elements is the same as the number of disabled displays,
    // this means that there are no displays enabled. 
    if ($array_count != $i) {

      $output .= "<div id='islandora_solr_secondary_display_profiles'>";

      foreach ($secondary_array as $name => $status) {
        if ($status === $name) {

          $description = $secondary_display_profiles[$name]['description'];
          $logo = $secondary_display_profiles[$name]['logo'];

          $attributes = array('title' => $description);
          $query = array();
          $query[] = drupal_query_string_encode(array('solr_profile' => $name));
          $output .= l($logo, $_GET['q'], array('attributes' => $attributes, 'html' => TRUE, 'query' => count($query) ? implode('&', $query) : NULL));
        }
      }

      $output .= "</div>";
    }

    $output .= '<h2>' . $title . '</h2>';
    $output .= '<div id="islandora_solr_result_count">(' . ($solrQueryProcessor->solrStart + 1) . ' - ' . $end . ' of ' . $total . ')</div>';
    $output .= '<div class="content">';

//    drupal_set_message(print_r($secondary_display_profiles,1));

    /*
     * Then, here, we need to build links to the current query under the secondary query display
     * profile for each such profile which uses the hook.
     *
     * The link should basically be $theCurrentURL with &proile=<profile_name> tacked on.
     *
     * Also, perhaps there's no difference between primary and secondary profiles?
     */

    $output .= $this->printResults($solrQueryProcessor->solrResult);

    // Render Drupal pager (bottom of search results)
    $output .= theme('pager', NULL, $solrQueryProcessor->solrLimit, 0, NULL, 5);


    $output .= "</div>"; // class = "content"
    return $output;
  }

  /**
   * Function: printResults
   *
   * Description: translates a solr query result into a basic in-browser search result.
   *
   * @param Apache_Solr_Object $results
   */
  function printResults($results) {

    $items = array();
    $type = "ol";
    $title = NULL;
    $recordStart = $results->response->start;
    $limitResults = variable_get('islandora_solr_search_limit_result_fields', 0);
    $highlights = $results->highlighting;
    foreach ($highlights as $highlight) {
      $fields = get_object_vars($highlight);
      $keys = array_keys($fields);
      if (count($keys) > 0) {
        foreach ($highlight->$keys[0] as $body) {
          //drupal_set_message("$keys[0]  $body");
        }
      }
      //drupal_set_message($highlight->$keys[0]);
    }

    global $base_url;
    if (empty($results)) {
      return "no results";
    }

    foreach ($results->response->docs as $doc) {
      $rows = array();
      $row = 0;

      foreach ($doc as $field => $value) {

        if ($limitResults && empty($this->resultFieldArray[$field])) {
          continue;
        }

        $translated_field_name = isset($this->allSubsArray[$field]) ? $this->allSubsArray[$field] : $field;
        $rows[$row][] = array(
          'data' => $translated_field_name,
          'header' => TRUE,
        );
        if (is_array($value)) {
          $value = implode(", ", $value);
        }

        if ($field == 'PID') {
          $l = l($value, 'fedora/repository/' . htmlspecialchars($value, ENT_QUOTES, 'utf-8'));
          $rows[$row][] = $l;
        }
        else {
          $rows[$row][] = $value;
        }
        $row++;
      }
      $items[] = theme_table(NULL, $rows, array('class' => 'islandora_solr_search_results_object'));
    }
    $output = theme_item_list($items, $title, $type, array('class' => 'stuff', 'start' => ($recordStart + 1)));
    if (variable_get('islandora_solr_search_debug_mode', 0)) { // debug dump
      $results_r .= "<pre>Results: " . print_r($results, TRUE) . "</pre>";
      $fieldset_r = array(
        '#title' => t("Raw Results"),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#value' => $results_r,
      );
      $output .= theme('fieldset', $fieldset_r);
    }
    return $output;
  }

  /**
   * Function: currentQuery
   *
   * Description: Displays elements of the current solr query (with enabled filters).
   *   This functionality was previously rolled into the facet block, but it makes
   *   sense to separate the two functions, particularly as the current query is now
   *   imprinted in the system's breadcrumb.
   *
   * @param IslandoraSolrQueryProcessor $solrQueryProcessor
   */
  function currentQuery($solrQueryProcessor) {
    //dsm($solrQueryProcessor);
    global $base_url;
    $output = '';
    $islandora_fq = restoreSlashes($solrQueryProcessor->solrFilters);
    $islandora_facets = isset($solrQueryProcessor->solrResult->facet_counts->facet_fields) ?
        $solrQueryProcessor->solrResult->facet_counts->facet_fields :
        NULL;
    //dsm($islandora_facets);
    if (strlen(trim($solrQueryProcessor->solrQuery))) {

      // Variables to build a drupal-ish unordered list
      $query_list_title = "Query";
      $query_list_items = array();
      $query_list_type = "ul";
      $query_list_attributes = array('class' => 'islandora_solr_search_query_list query_list');

      // OK, so it's a list of one.
      // In the future, we could reasonably break the query on boolean operators
      // and allow one part to be removed at a time.
      $query_list_items[] .= l(
              '<img class="islandora_solr_add_remove_link" src="' . $base_url . '/' . drupal_get_path('module', 'islandora_solr_search') . '/images/delete.png" alt="Remove search term">', 'islandora/solr/search/' . " " . '/' . replaceSlashes($islandora_fq) . '/dismax', array('html' => TRUE)
          ) . ' ' . stripslashes($solrQueryProcessor->solrQuery);

      $output .= '<div class="islandora_solr_search_query_wrap">';

      $output .= theme_item_list($query_list_items, $query_list_title, $query_list_type, $query_list_attributes);
      $output .= '</div>'; // 'islandora_solr_search_query_wrap'
    }

    //////////////////////////////////////////////////////////////////////////////
    // Here we will build links to remove currently enforced facet filters ...  //
    //////////////////////////////////////////////////////////////////////////////
    $filters = empty($islandora_fq) ? NULL : explode(IslandoraSolrResults::$facetSeparator, $islandora_fq);

    if (!empty($filters) && $islandora_fq != '-') {
//      $filter_output = NULL;
      // Variables to build a drupal-ish unordered list, which will be rendered
      // a little bit farther down
      $filter_list_title = "Enabled Filters";
      $filter_list_items = array();
      $filter_list_type = "ul";
      $filter_list_attributes = array('class' => 'islandora_solr_search_filter_list filter_list');

      foreach ($filters as $key => $filter) {
        $edit_filters = $filters;
        unset($edit_filters[$key]);
        $filter_disable = implode(IslandoraSolrResults::$facetSeparator, $edit_filters);
        if (empty($filter_disable)) {
          $filter_disable = '-';
        }
        $filter_name_and_value = explode(":", $filter, 2);
        $filter_name = $filter_name_and_value[0];
        $filter_value = $filter_name_and_value[1];
        if (strncmp($filter_name, '-', 1))
          $exclude = FALSE;
        else {
          $exclude = TRUE;
          $filter_name = substr($filter_name, 1);
        }
        $filter_list_items[] = l(
                '<img class="islandora_solr_add_remove_link" src="' . $base_url . '/' . drupal_get_path('module', 'islandora_solr_search') . '/images/delete.png" alt="Remove this filter">', 'islandora/solr/search/' . replaceSlashes($solrQueryProcessor->solrQuery) . '/' . replaceSlashes($filter_disable) . '/' . $solrQueryProcessor->solrDefType, array('html' => TRUE)
            ) . ' ' . $this->facetFieldArray[$filter_name] . ($exclude ? ' != ' : ' = ') . $filter_value;
      }
    }

    if (!empty($filter_list_items)) {
//      $filter_output = str_replace('%2B','%252B',$filter_output);
      $output .= '<div class="islandora_solr_search_filter_wrap">';
      $output .= theme_item_list($filter_list_items, $filter_list_title, $filter_list_type, $filter_list_attributes);
      $output .= '</div>'; // 'islandora_solr_search_filter_wrap'
    }

    return $output;
  }

  /**
   * Function: prepFieldSubstitutions
   *
   * Description: reads configuration values and preps a number of key=>value
   *   arrays for output substitution
   */
  function prepFieldSubstitutions() {
    $rawFacetVals = variable_get("islandora_solr_search_block_facets", 'dc.subject ~ Subject,dc.type ~ Type');
    $this->facetFieldArray = islandora_build_substitution_list($rawFacetVals);

    $rawSearchTerms = variable_get('islandora_solr_searchterms', 'dc.title ~ Title');
    $this->searchFieldArray = islandora_build_substitution_list($rawSearchTerms);

    $rawResultFields = variable_get('islandora_solr_search_result_fields', 'dc.subject ~ Subject,dc.type ~ Type');
    $this->resultFieldArray = islandora_build_substitution_list($rawResultFields);

    $this->allSubsArray = array_merge($this->facetFieldArray, $this->searchFieldArray, $this->resultFieldArray);
  }

  /**
   * Function: displayFacets
   *
   * Description: Displays basic facets based on an apache solr query response,
   *   as contained with the IslandoraSolrQueryProcessor.
   *
   * @global string $base_url
   * @param IslandoraSolrQueryProcessor $solrQueryProcessor
   * @return string
   */
  function displayFacets($solrQueryProcessor) {
    global $base_url;
    $facet_output = $output = '';
    $islandora_fq = $solrQueryProcessor->solrFilters;
    $islandora_facets = isset($solrQueryProcessor->solrResult->facet_counts->facet_fields) ? $solrQueryProcessor->solrResult->facet_counts->facet_fields : array();

    if (empty($islandora_facets)) {
      return $output; //no facets to show
    }
    foreach ($islandora_facets as $key => $field) {
      $list_items = array();
      $list_type = "ul";
      $list_title = NULL;

      $test = get_object_vars($field); //get the number of fields if there aren't any don't show the key
      if (count($test) > 0) {
        $facet_count = 0;
        unset($normal_facet_output);
        $filter_include = NULL;
        $filter_exclude = NULL;
        $disable_link = FALSE;
        foreach ($field as $name => $number) {
          if ($islandora_fq && $islandora_fq != '-') {//there are existing facets in the query
            $disable_link = strstr($islandora_fq, $key . ':"' . replaceSlashes($name) . '"'); //we don't want a link for this facet as we already used it
            if ($disable_link) {
              //don't show link to this facet but include a link to remove it
            }
            else {
              $filter_include = $key . ':' . '"' . $name . '"' . IslandoraSolrResults::$facetSeparator . $islandora_fq;
              $filter_exclude = '-' . $key . ':' . '"' . $name . '"' . IslandoraSolrResults::$facetSeparator . $islandora_fq;
            }
          }
          else { //no existing facets chosen
            $filter_include = $key . ':' . '"' . $name . '"';
            $filter_exclude = '-' . $key . ':' . '"' . $name . '"';
          }

          $filter_include = replaceSlashes($filter_include); //replace the slash so url does not break
          $filter_exclude = replaceSlashes($filter_exclude); //replace the slash so url does not break
          if ($disable_link) {
            // we don't want to create a link, because we're already filtering on this value.
            // Links to remove enabled facet filters are created down below.
          }
          else {//normal link
            $evenodd = ( $facet_count % 2 ? 'odd' : 'even' );
            $lplus = 'islandora/solr/search/' . $solrQueryProcessor->solrQuery . '/' . $filter_include . '/' . $solrQueryProcessor->solrDefType;
            $lminus = 'islandora/solr/search/' . $solrQueryProcessor->solrQuery . '/' . $filter_exclude . '/' . $solrQueryProcessor->solrDefType;
            $text = $name;
            $attrs = array();
            $list_items[] = l($text, $lplus, $attrs) . ' (' . $number . ')' .
                '<span class="plusminus">' .
                l('+', $lplus, array('attributes' => array('class' => 'plus'))) . ' ' .
                l('-', $lminus, array('attributes' => array('class' => 'minus'))) .
                '</span>';
            $facet_count++;
          }
        }
        if ($facet_count >= 2) {
          $facet_output .='<div class="islandora_solr_search_facet">';
          $list_title = $this->facetFieldArray[$key];
          $list_type = "ul";
          $list_attributes = array('class' => 'islandora_solr_search_facet_list facet_list');
          $facet_output .= theme_item_list($list_items, $list_title, $list_type, $list_attributes);
          $facet_output .='</div>';
        }
      }
    }


    //////////////////////////////////////////////////////////////////////////////////////
    // as we add additional facets, we're repeatedly URL-encoding old facet             //
    // strings. when we double-encode quotation marks they're incomprehensible to solr. //
    // This is a quick workaround:                                                      //
    //////////////////////////////////////////////////////////////////////////////////////
    $facet_output = str_replace('%2B', '%252B', $facet_output);
    $output .='<div class="islandora_solr_search_facet_wrap">';
    $output .= $facet_output;
    $output .= '</div>';

    return $output;
  }

  /**
   * Build solr search form
   * @return array
   */
  function build_solr_search_form() {

    $terms = trim(variable_get('islandora_solr_searchterms', 'dc.title ~ Title,dc.subject ~ Subject'));
    $termsArray = preg_split('/[\n]/', $terms);
    foreach ($termsArray as $term) {
      $vals = explode('~', $term);
      if (!$vals[1]) {
        $vals[1] = $vals[0];
      }
      $types[trim($vals[0])] = trim($vals[1]);
    }

    $andOrArray = array(
      'AND' => 'and',
      'OR' => 'or',
    );
    $form = array();

    $repeat = variable_get('islandora_solr_search_block_repeat', '3');
    $form['redirect'] = FALSE;
    $form['type1'] = array(
      '#title' => '',
      '#type' => 'select',
      '#options' => $types,
    );
    $form['fedora_terms1'] = array(
      '#size' => '24',
      '#type' => 'textfield',
      '#title' => '',
     
    );
  
    if ($repeat > 1 && $repeat < 9) { //don't want less then 2 or more then 9
      for ($terms = 2; $terms <= $repeat; $terms++) {
        $bool = $terms -1;
        $form["andor$bool"] = array(
          '#title' => '',
          '#type' => 'select',
          '#options' => $andOrArray
        );
        $form["type$terms"] = array(
          '#title' => '',
          '#type' => 'select',
          '#options' => $types,
        );
        $form["fedora_terms$terms"] = array(
          '#size' => '24',
          '#type' => 'textfield',
          '#title' => '',
        );
      }
    }

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('search')
    );

    return $form;
  }


  /**
   * Build simple solr form
   * @global type $islandora_query
   * @return type 
   */
  function build_simple_solr_form($form, &$forms_state) {
    $required = FALSE;
    if (variable_get('islandora_solr_search_block_request_handler', '') == 'standard') {
      $required = TRUE;
    }
    global $islandora_query;
    if (!strpos($islandora_query, ':')) {
      $default = $islandora_query;
    }
    // TODO: Fix the above.  The global is no longer valid, and the test is not conclusive.  Should instead test for deftype=dismax.
    $form["islandora_simple_search_query"] = array(
      '#size' => '24%',
      '#type' => 'textfield',
      '#title' => '',
      '#default_value' => $default,
      '#required' => $required,
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('search')
    );
    return $form;
  }

  /**
   * Theme solr search form
   * @param type $form
   * @return type 
   */
  function theme_solr_search_form($form) {
    if (!isset($repeat)) {
      $repeat = variable_get('islandora_solr_search_block_repeat', '3');
    }

    $output = drupal_render($form['search_type']['type1']);
    $output .= drupal_render($form['fedora_terms1']);
    $output .= drupal_render($form['andor1']) . drupal_render($form['search_type']['type2']);
    $output .= drupal_render($form['fedora_terms2']);
    if ($repeat > 2 && $repeat < 9) {
      for ($i = 3; $i < $repeat + 1; $i++) {
        $t = $i - 1;
        $output .= drupal_render($form["andor$t"]) . drupal_render($form['search_type']["type$i"]);
        $output .= drupal_render($form["fedora_terms$i"]);
      }
    }
    $output .= drupal_render($form['submit']);
    $output .= drupal_render($form);

    return $output;
  }

}

