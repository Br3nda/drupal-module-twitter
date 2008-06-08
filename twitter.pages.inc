<?php
// $Id: twitter.pages.inc,v 1.1 2008/06/08 04:48:33 walkah Exp $

/**
 *
 */
function twitter_admin_form() {
  $form = array();
  
  $form['twitter_default_format'] = array(
    '#type' => 'textfield',
    '#title' => t('Default format string'),
    '#maxlength' => 140,
    '#description' => t('The given text will be posted to twitter.com. You can use !url, !title and !user as replacement text.')
  );
  
  $form['twitter_types'] = array(
    
  );
 
  return system_settings_form($form); 
}

function twitter_user_settings($account) {
  module_load_include('inc', 'twitter');
  
  $twitter_accounts = twitter_get_user_accounts($account->uid);
  
  $header = array('', t('Name'), t('Description'));
  $rows = array();
  
  foreach ($twitter_accounts as $twitter_account) {
    $rows[] = array(
      '<img src="'.$twitter_account['profile_image_url'].'" />',
      $twitter_account['screen_name'],
      $twitter_account['description'],
    );
  }

  $output = theme('table', $header, $rows);
  $output .= drupal_get_form('twitter_add_account', $account);
  return $output;
}

function twitter_add_account($form_state, $account = NULL) {
  if (empty($account)) {
    global $user;
    $account = $user;
  }
  
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $account->uid,
  );
  $form['screen_name'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Twitter user name'),
  );
  $form['password'] = array(
    '#type' => 'password',
    '#required' => TRUE,
    '#title' => t('Password'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add account'),
  );

  return $form;  
}

function twitter_add_account_validate($form, &$form_state) {
  module_load_include('inc', 'twitter');
  
  $verify = FALSE;

  $pass = $form_state['values']['password'];
  $name = $form_state['values']['screen_name'];

  if (!empty($pass)) {
    $verify = TRUE;
  }

  if ($verify) {
    $valid = twitter_authenticate($name, $pass);
    if (!$valid) {
      form_set_error("password", t('Twitter authentication failed. Please check your account name and try again.'));
    }
  }
}

function twitter_add_account_submit($form, &$form_state) {
  module_load_include('inc', 'twitter');
   
  if (!empty($form_state['values']['screen_name'])) {
    twitter_user_save($form_state['values']);
  }
}
