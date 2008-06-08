<?php
// $Id: twitter.inc,v 1.1 2008/06/08 04:48:33 walkah Exp $

/**
 * Generate a twitter posting form for the given user.
 *
 * @param object $account user account object.
 */
function twitter_form($account = NULL) {
  drupal_add_js(drupal_get_path('module', 'twitter') .'/twitter.js', 'module');
  
  if (empty($account)) {
    global $user;
    $account = $user;
  }
  
  $twitter_accounts = drupal_map_assoc(array_keys(twitter_get_user_accounts($account->uid)));
  if (count($twitter_accounts)) {
    $form = array();
    $form['status'] = array(
      '#type' => 'textarea',
      '#rows' => 1,
      '#id' => 'twitter-textarea',
    );
    
    if (count($twitter_accounts) > 1) {
      $form['account'] = array(
        '#type' => 'select',
        '#title' => t('Account'),
        '#options' => $twitter_accounts,
        '#access' => user_access(''),
        '#id' => 'twitter-account',
      );
    }
    else {
      $form['account'] = array(
        '#type' => 'value',
        '#value' => array_pop(array_keys($twitter_accounts))
      );
    }
    return $form;
  }
}

/**
 * Twitter API functions
 */
function twitter_set_status($screen_name, $password, $text = '', $source = NULL) {
  $url = "http://twitter.com/statuses/update.xml";

  $headers = array('Authorization' => 'Basic '. base64_encode($screen_name .':'. $password),
                   'Content-type' => 'application/x-www-form-urlencoded');
  $data = 'status='. urlencode($text);
  if (!empty($source)) {
    $data .= 'source='. urlencode($source);
  }

  return drupal_http_request($url, $headers, 'POST', $data);
}

function twitter_fetch_user_timeline($screen_name, $filter_since = TRUE, $cache = TRUE) {
  if ($filter_since) {
    $sql  = "SELECT t.created_at FROM {twitter} t WHERE t.screen_name = '%s' ORDER BY t.created_at DESC";
    $since = db_result(db_query($sql, $screen_name));
  }

  $url = "http://twitter.com/statuses/user_timeline/$screen_name.xml";

  if (!empty($since)) {
    $url .= '?since='. urlencode($since);
  }

  $results = drupal_http_request($url, array(), 'GET');
  if ($results->code == 304) {
    return array();
  }
  else {
    $results = _twitter_convert_xml_to_array($results->data);
    if ($cache) {
      foreach($results as $status) {
        twitter_cache_status($status);
      }
    }
    return $results;
  }
}

function twitter_fetch_user_friends($screen_name, $cache = TRUE) {
  $url = "http://twitter.com/statuses/friends/$screen_name.xml";
  $results = drupal_http_request($url, array(), 'GET');
  return _twitter_convert_xml_to_array($results->data);
}

function twitter_fetch_user_followers($screen_name, $password, $cache = TRUE) {
  $url = "http://twitter.com/statuses/followers/$screen_name.xml";
  $headers = array('Authorization' => 'Basic '. base64_encode($screen_name .':'. $password),
                   'Content-type' => 'application/x-www-form-urlencoded');
  $results = drupal_http_request($url, $headers, 'GET');
  $results = _twitter_convert_xml_to_array($results->data);

  if ($cache) {
    foreach($results as $status) {
      twitter_cache_status($status);
    }
  }
  return $results;
}

function twitter_fetch_status($screen_name, $cache = TRUE) {
  $url = "http://twitter.com/statuses/$screen_name.xml";
  $results = drupal_http_request($url, array(), 'GET');
  $results = _twitter_convert_xml_to_array($results->data);

  if ($cache && !empty($results)) {
    foreach($results as $status) {
      twitter_cache_status($status);
      return $status;
    }
  }
}

function twitter_authenticate($screen_name, $password) {
  $url = "http://twitter.com/account/verify_credentials.xml";
  $headers = array('Authorization' => 'Basic '. base64_encode($screen_name .':'. $password),
                   'Content-type' => 'application/x-www-form-urlencoded');
  $results = drupal_http_request($url, $headers, 'GET');
  drupal_http_request('http://twitter.com/account/end_session', $headers, 'GET');
  return ($results->code == '200');
}

function twitter_fetch_user_details($screen_name, $password, $cache = TRUE) {
  $url = "http://twitter.com/users/show/$screen_name.xml";
  $headers = array('Authorization' => 'Basic '. base64_encode($screen_name .':'. $password),
                   'Content-type' => 'application/x-www-form-urlencoded');
  $results = drupal_http_request($url, $headers, 'GET');
  if ($results->code == 401) {
    return array();
  }
  $results = _twitter_convert_xml_to_array($results->data);

  if ($cache) {
    foreach($results as $user) {
      twitter_cache_user($user);
    }
  }
  return $results;
}


/**
 * Helper functions
 */
function twitter_cache_user($twitter_user = array()) {
  db_query("DELETE FROM {twitter_account} WHERE twitter_uid = %d", $twitter_user['twitter_uid']);
  drupal_write_record('twitter_account', $twitter_user);
}

function twitter_get_user_accounts($uid) {
  $sql = "SELECT *, tu.screen_name AS screen_name FROM {twitter_user} tu LEFT JOIN {twitter_account} ta ON (tu.screen_name = ta.screen_name) WHERE tu.uid = %d";
  $args = array($uid);
  $results = db_query($sql, $args);

  $accounts = array();
  while ($account = db_fetch_array($results)) {
    $accounts[$account['screen_name']] = $account;
  }
  return $accounts;
}

function twitter_user_save($user = array()) {
  $user += array(
    'screen_name'       => '',
    'password'          => '',
    'last_refresh'      => 0,
    'show_on_profile'   => 1,
    'show_in_blocks'    => 1,
    'announce_types'        => array(),
    'announce_message'      => '',
  );
  
  $user['announce_types'] = serialize($user['announce_types']);
  if (db_result(db_query("SELECT 1 FROM {twitter_user} WHERE uid = %d AND screen_name = '%s'", $user['uid'], $user['screen_name']))) {
    drupal_write_record('twitter_user', $user, array('uid', 'screen_name'));
  }
  else {
    drupal_write_record('twitter_user', $user);
  }
  $user['announce_types'] = unserialize($user['announce_types']);

  twitter_fetch_user_details($user['screen_name'], $user['password']);
}

function twitter_user_delete($uid, $screen_name = NULL) {
  $sql = "DELETE FROM {twitter_user} WHERE uid = %d";
  $args = array($uid);
  if (!empty($screen_name)) {
    $sql .= " AND tu.screen_name = '%s'";
    $args[] = $screen_name;
  }
  db_query($sql, $args);
}

/**
 * Internal XML munging code
 */

function _twitter_convert_xml_to_array($data) {
  $results = array();
  $xml = new SimpleXMLElement($data);
  if (!empty($xml->name)) {
    // Top-level user information.
    $results[] = _twitter_convert_user($xml);
    return $results;
  }
  if (!empty($xml->user)) {
    foreach($xml->user as $user) {
      $results[] = _twitter_convert_user($user);
    }
  }
  elseif (!empty($xml->status)) {
    foreach($xml->status as $status) {
      $results[] = _twitter_convert_status($status);
    }
  }
  return $results;
}

function _twitter_convert_status($status) {
  $result = (array)$status;
  $result['twitter_id'] = $result['id'];
  if (!empty($result['user']) && is_object($result['user'])) {
    $result['account'] = _twitter_convert_user($result['user']);
    $result['screen_name'] = $result['account']['screen_name'];
  }
  else {
    $result['screen_name'] = NULL;
  }
  $result['created_time'] = strtotime($result['created_at']);
  return $result;
}

function _twitter_convert_user($user) {
  $result = (array)$user;
  $result['twitter_uid'] = $result['id'];
  if (!empty($result['status']) && is_object($result['status'])) {
    $result['status'] = _twitter_convert_status($result['status']);
  }
  else {
    $result['twitter_uid'] = NULL;
  }
  return $result;
}

function _twitter_account_fields($user, $account = array()) {
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $user->uid,
  );
  $form['screen_name'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Twitter user name'),
    '#default_value' => $twitter['screen_name'],
  );
  $form['password'] = array(
    '#type' => 'password',
    '#required' => TRUE,
    '#title' => t('Password'),
    '#default_value' => $twitter['password'],
  );

  return $form;
}
