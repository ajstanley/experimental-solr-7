<?php

/**
 * @file
 * Contains methods to build and execute a solr query.
 * Depends on Apache_Solr_Php client.
 */

/**
 * Islandora Solr Query Processor
 * @todo Stop using global vars, start using this object's vars.
 *  The module itself should initially create an instance of this
 *  object and reference the query vars inside that object when calling
 *  other output display functions.
 */
class IslandoraSolrQueryProcessor {

  static $facetSeparator = '~'; //used to separate facets in url
  static $slashReplacement = '~slsh~'; // a pattern used to replace / in url's the slash breaks drupal clean url's
  public static $SEARCH_CLASS_ADVANCED_SEARCH_NUMBER_FIELDS = 5;
  public $solrQuery;
  public $solrParams;
  public $solrStart;
  public $solrLimit;
  public $solrDefType;
  public $solrFilters;
  public $solrResult;

  /**
   * Constructor
   */
  function IslandoraSolrQueryProcessor() {
    module_load_include('php', 'islandora_solr_search', 'SolrPhpClient/Apache/Solr/Service');
    module_load_include('inc', 'islandora_solr_search', 'includes/common');
  }

  /**
   * Build and execute a query
   * @param type $query
   * @param type $fq
   * @param type $dismax
   * @return type 
   */
  function buildAndExecuteQuery($query, $fq=NULL, $dismax=NULL) {

    if (variable_get('islandora_solr_search_block_request_handler', NULL) == 'standard') {
      $namespace = trim(variable_get('solr_namespace_restriction', NULL));
      if (!$query || $query == ' ') {
        $query = '%252F';
      }

      if ($namespace) {
        if ($fq == '-') {
          $fq = '';
        }
        if (strpos($fq, "PID:$namespace") === FALSE) {
          if ($fq) {
            $fq .= $this->facetSeparator;
          }
          $fq .= "PID:$namespace*";
        }
      }
    }
    $this->buildQuery($query, $fq, $dismax);
    return $this->executeQuery();
  }

  /**
   * Default implementation for solr search.  You can configure others in the block config settings
   * @todo Get rid of the global $facetArray?  It is the only global yet relied upon.
   */
  function buildQuery($query, $fq=NULL, $dismax=NULL) {
    // Get pager variable
    $startPage = isset($_GET['page']) ? $_GET['page'] : 0;
    $this->solrLimit = variable_get('islandora_solr_search_num_of_results', 20);
    global $base_url, $facetArray; //used by facet block as well
    // -- cut here --
    // Get display profile
    $islandora_solr_primary_display = variable_get('islandora_solr_primary_display', 'default');
    $profiles = module_invoke_all("islandora_solr_primary_display");
    if (!isset($profiles[$islandora_solr_primary_display])) {
      drupal_set_message("There is an error in the solr search configuration: the display profile is not found.", 'error');
      return "";
    }
    $profile = $profiles[$islandora_solr_primary_display];

    // Include the file for the display profile
    require_once(drupal_get_path('module', $profile['module']) . '/' . $profile['file']);

    // Set display class and function vars
    $solrClass = $profile['class'];
    $solrFunction = $profile['function'];

    // -- end cut here --
    //fix the query as some characters will break the search : and / slash are examples
    $this->solrQuery = restoreSlashes($query);

    if (empty($this->solrQuery)) {
      // $this->solrQuery = '%20'; //so we can allow empty queries to dismax
    }

    $facetArray = array();
    $facetFields = '';
    $rawFacetVals = variable_get("islandora_solr_search_block_facets", 'dc.subject,dc.type');
    $facetArray = islandora_build_substitution_list($rawFacetVals);
    $facetFields = implode(",", array_keys($facetArray));
    $keys = array('');
    $snippetArray = islandora_build_substitution_list(variable_get("islandora_solr_snippet_field", ""));
    if (is_array($snippetArray)) {
      $keys = array_keys($snippetArray);
      if (!isset($keys[0])) {
        $keys[0] = '';
      }
    }

    $facetlimit = variable_get('islandora_solr_search_block_facet_limit', '12');
    $facetMinCount = variable_get('islandora_solr_search_block_facet_min_count', '2');
    $requestHandler = variable_get("islandora_solr_search_block_request_handler", "standard");
    $highlight_field = variable_get("islandora_solr_snippet_field", "");
    $this->solrParams = array(
      'facet' => 'true',
      'facet.mincount' => $facetMinCount,
      'facet.limit' => $facetlimit,
      'qt' => $requestHandler,
      'hl' => 'true',
      'hl.fl' => trim($keys[0]),
      'hl.fragsize' => 400,
      'facet.field' => explode(',', $facetFields), //comma separated list configured in the block config
    );


    $debugQuery = (variable_get('islandora_solr_search_debug_mode', 0) ? "TRUE" : NULL ); //any val incl. 'FALSE' is treated as TRUE by Solr

    if ($fq != NULL && $fq != '-') {
      $fq = restoreSlashes($fq); //put the slash back

      $fqs = csv_explode(IslandoraSolrQueryProcessor::$facetSeparator, $fq, '"', TRUE); //to filter by more then one facet we will separate them by~ for nowseparate them by~ for now
      $this->solrParams['fq'] = $fqs;
      $islandora_fq = replaceSlashes($fq); //remove the slash here as we will be using this in url's
    }
    if (empty($islandora_fq)) {
      $islandora_fq = '-';
    }
    if ($dismax != NULL) {
      $this->solrDefType = $dismax;
      $this->solrParams['defType'] = $dismax;
    }
    $this->solrStart = max(0, $startPage) * $this->solrLimit;

    // The breadcrumb should go in the display class
    $queryurl = "islandora/solr/search/" . replaceSlashes($this->solrQuery);
    $breadcrumb_fq = $islandora_fq;
    $facet = NULL; // Nigelb, This is being used outside of the for loop where it is being created this is probably incorrect.
    if (strcmp($islandora_fq, "-")) {
      foreach (csv_explode(IslandoraSolrQueryProcessor::$facetSeparator, $islandora_fq, '"', TRUE) as $facet) {
        $field = substr($facet, 0, strpos($facet, ":"));
        $value = restoreSlashes(trim(substr($facet, strpos($facet, ":") + 1), '"'));
        $options = array('html' => TRUE);
        $options['attributes']['title'] = $facet;
        $crumblink = $queryurl . "/" . $breadcrumb_fq . (empty($this->solrDefType) ? '' : '/' . $this->solrDefType);
        $cut_fq = $this->delete_filter($islandora_fq, $facet);
        $cutlink = $queryurl . "/" . $cut_fq . (empty($this->solrDefType) ? '' : '/' . $this->solrDefType);
        if (!strncmp("-", $facet, 1)) {
          $options['attributes']['class'] = "strikethrough";
        }
        $breadcrumb[] = l($value, $crumblink, $options) .
            " <span class='islandora_solr_search_breadcrumb_super'>(" . l("x", $cutlink, array('attributes' => array('title' => "Remove " . $facet))) . ")</span>";
        $breadcrumb_fq = $this->delete_filter($breadcrumb_fq, $facet);
      }
    }
    if (!empty($this->solrQuery) && strcmp($this->solrQuery, ' ')) {
      $cutlink = "islandora/solr/search/ /" . $islandora_fq . "/dismax";
      $queryval = $this->solrQuery;
      $tokens = explode(" ", $queryval);
      foreach ($tokens as $token) {
        preg_match("/^.*:/", $token, $matches);
        if (isset($matches[0])) {
          $queryval = str_replace($matches[0], "", $queryval);
        }
      }
      if ($queryval != '%252F') {
        $breadcrumb[] = l($queryval, $queryurl . "/-", array('attributes' => array('title' => $this->solrQuery))) .
            " <span class='islandora_solr_search_breadcrumb_super'>(" . l("x", $cutlink, array('attributes' => array('title' => "Remove " . $facet))) . ")</span>";
      }
    }
    $breadcrumb[] = l(t('Home'), NULL);
    if (!empty($breadcrumb))
      $breadcrumb = array_reverse($breadcrumb);
    drupal_set_breadcrumb($breadcrumb);

    $this->solrFilters = $islandora_fq;

    // At this point let's invoke a hook for third-party modules to mess about
    // with the query parameters if they need to.  Third party modules may alter
    // this object's query parameters if they wish.
    // module_invoke_all("islandora_solr_search_query_processor", &$this);
    return;
  }

  /**
   * Reset Results
   */
  function resetResults() {
    unset($this->solrResult);
  }

  /**
   * Execute the query
   * @return type 
   */
  function executeQuery() {

    // //////////////////////////////////////////////////////////////////////////// //
    // Right here the function should be split.  One function to execute the query, //
    // another to set up pager variables and generate display.                      //
    // //////////////////////////////////////////////////////////////////////////// //

    $url = variable_get('islandora_solr_search_block_url', 'http://localhost:8080/solr');
    $pathParts = parse_url($url);

    $solr = new Apache_Solr_Service($pathParts['host'], $pathParts['port'], $pathParts['path'] . '/');
    $solr->setCreateDocuments(0);

    // This is where the query gets executed and output starts being created.
    try {
      $results = $solr->search($this->solrQuery, $this->solrStart, $this->solrLimit, $this->solrParams);
    } catch (Exception $e) {
      drupal_set_message(t('error searching ') . $e->getMessage());
    }
    $this->solrResult = $results;
    if (empty($results)) {
      drupal_set_message(t('Error searching solr index. Is the solr search block configured properly?'), 'error');
      return;
    }

    unset($results);
    unset($solr);

    return;
  }

  /**
   * Function delete_filter
   *
   * Deletes one filter from a multi-filter string
   *
   * @param <string> $facets
   * @param <string> $facet
   */
  function delete_filter($filters, $filter) {
    $filters = str_replace($filter, "", $filters);
    $filters = str_replace("~~", "~", $filters);
    $filters = trim($filters, "~");
    return $filters;
  }

}

