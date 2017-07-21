<?php
/**
 * @file
 * This file contains the admin and GUI callbacks for the module that are not needed in day-to-day operations
 */

define('WFET_CREATE_NEW_FIELD', -1);
define('WFET_LEAVE_FIELD_BLANK', -2);
define('WFET_CREATE_HIDDEN_FIELD', -3);

/*
 * Searches for a best match for default values
 */
function _webform_exact_target_best_match($field, $options) {
  $pct_match = 0;
  $best_match = 0;
  $pct_best_match = 0;
  foreach ($options as $key => $val) {
    similar_text($field, $val, $pct_match);
    if ($pct_match > $pct_best_match) {
      $best_match = $key;
      $pct_best_match = $pct_match;
    }
  }

  return ($pct_best_match > 75) ? $best_match : NULL;
}


function webform_exact_target_configure_page($node) {
  return drupal_get_form('webform_exact_target_configure_form', $node);
}

/**
 * Implements callback for form construction
 */
function webform_exact_target_configure_form($form, $form_state, $node) {
  $form = array();
  $form_cfg = _webform_exact_target_get_mappings($node);
  $mappings = (!empty($form_cfg['mappings'])) ? $form_cfg['mappings'] : array();
  $def_listids = (!empty($form_cfg['listid'])) ? $form_cfg['listid'] : array();

  // Get the ExactTarget profile attributes
  $all_attrs = exact_target_api_get_attributes();
  $named_attrs = array();
  foreach ($all_attrs->results AS $attr) {
    $named_attrs[$attr->Name] = $attr;
  }
  // we have a long list of attributes, but the data doesn't tell us which
  // attributes are part of the subscriber profile, so we load an example
  // subscriber to see what attributes are in the profile
  $example_subscriber = webform_exact_target_example_subscriber();
  if (is_object($example_subscriber->Attributes)) {
    $example_subscriber->Attributes = array($example_subscriber->Attributes);
  }
  $email_attr = $named_attrs['EmailAddress'];
  $email_attr->Name = 'SubscriberEmail';
  $email_attr->CustomerKey = 'SubscriberEmail';
  $email_attr->IsRequired = TRUE;
  unset($email_attr->DataExtension);
  $profile_attrs = array($email_attr);
  foreach ($example_subscriber->Attributes AS $attr) {
    $profile_attrs[] = $named_attrs[$attr->Name];
  }
  
  if (!empty($profile_attrs)) {
    // Get all available Webform fields and build option list for mapping

    $wf_fields = array(
      WFET_LEAVE_FIELD_BLANK => t('Leave Field Blank'),
      // TODO implement this
      // WFET_CREATE_NEW_FIELD => t('Create New Webform Field'),
    );
    $wf_fields_req = array(
      // TODO implement this
      // WFET_CREATE_NEW_FIELD => t('Create New Webform Field'),
      WFET_CREATE_HIDDEN_FIELD => t('Create New Hidden Webform Field'),
    );
    foreach ($node->webform['components'] as $field) {
      $name = $field['name'];
      if ($field['mandatory']) {
        $name .= ' *';
      }
      $wf_fields[$field['form_key']] = $name;
      if ($field['required']) {
        $wf_fields_req[$field['form_key']] = $name;
      }
    }

    // Get all available ET lists and build option list:
    $tmp_lists = exact_target_api_get_lists();
    $et_list_groups = array();

    foreach ($tmp_lists as $listid => $list_info) {
      //$list_info = exact_target_api_get_list_info($listid);
      $et_list_groups[$list_info->Type][$listid] = t('Subscribe to emails from %listname', array('%listname' => $list_info->ListName));
    }
    ksort($et_list_groups);

    $form['wfet_list_groups'] = array(
      '#type' => 'fieldset',
      '#title' => t('ExactTarget Subscription Lists'),
      '#description' => t('Please select the target subscription list(s) that may be subscribed to via this Webform'),
      '#tree' => TRUE,
    );

    foreach ($et_list_groups as $type => $et_lists) {
      $form['wfet_list_groups'][$type] = array(
        '#type' => 'fieldset',
        '#title' => check_plain($type),
        '#tree' => TRUE,
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      $form['wfet_list_groups'][$type]['listid'] = array(
        '#type' => 'checkboxes',
        '#options' => $et_lists,
        '#default_value' => $def_listids,
        '#required' => FALSE,
      );
    }

    $form['mappings'] = array(
      '#tree' => TRUE,
    );
    foreach ($profile_attrs as $attr) {
      if (substr($attr->Name, 0,1) == '_') {
        continue;
      }
      $defval = (!empty($mappings[$attr->CustomerKey])) ? $mappings[$attr->CustomerKey] : _webform_exact_target_best_match($attr->Name, $wf_fields);
      $form['mappings'][$attr->Name] = array(
        '#type' => 'select',
        '#title' => check_plain($attr->Name),
        '#options' => $attr->IsRequired ? $wf_fields_req : $wf_fields,
        '#default_value' => $defval ?: ( $attr->IsRequired ? WFET_CREATE_NEW_FIELD : WFET_LEAVE_FIELD_BLANK),
        '#required' => $attr->IsRequired ? TRUE : FALSE,
      );
      $form['#attribute_specs'][$attr->Name] = $attr;
    }

    // Set node info as it doesn't get passed to the submit handler

    $form['nid'] = array(
      '#type' => 'hidden',
      '#value' => $node->nid,
    );
    $form['vid'] = array(
      '#type' => 'hidden',
      '#value' => (!empty($node->vid)) ? $node->vid : 0,
    );

    $form['wfet_mappings_info'] = array(
      '#type' => 'markup',
      '#value' => t('<p>Select the Webform field that should be mapped to the ExactTarget profile attribute.<br />Note that ExactTarget attributes that are required will only map to a Webform required field.<br />All required fields are designated with a <strong>*</strong> next to their name.</p>'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    $form['clear'] = array(
      '#type' => 'submit',
      '#value' => 'Reset Form',
      '#validate' => array('webform_exact_target_configure_form_clear'),
    );
  }
  else {
    $form['msg'] = array(
      '#type' => 'markup',
      '#value' => t('No profile attributes are defined for this account, or else there was an error retrieving the attribute list.  Please !check_log_link for errors.', array('!check_log_link' => l(t('check the log'), 'admin/reports/dblog'))),
    );
  }

  return $form;
}

/**
 * Implements theme callback for form rendering
 */
function theme_webform_exact_target_configure_form($variables) {
  $form = $variables['form'];
  $output = '';

  // Render list select:

  $output .= drupal_render($form['wfet_list_groups']);

  if (!empty($form['mappings'])) {
    // Render fields as two-column table:

    $header = array(t('ExactTarget Field'), t('Webform Field'));
    $rows = array();
    foreach (element_children($form['mappings']) as $id) {
      $row = array();
      $field = &$form['mappings'][$id];
      $label = $field['#title'];
      if ($field['#required']) {
        $label .= ' <span class="form-required" title="This field is required.">*</span>';
      }
      $row[] =  $label;
      $field['#title'] = '';
      $row[] = drupal_render($field);
      $rows[] = $row;
    }
    $output .= theme('table', array('header' => $header, 'rows' => $rows));
  }

  // Render rest of the form elements

  $output .= drupal_render_children($form);

  return $output;
}

/**
 * Validate function for our reset button
 */
function webform_exact_target_configure_form_clear($form, &$form_state) {
  unset($form_state['values']);
  unset($form_state['storage']);

  $form_state['rebuild'] = TRUE;
}

/**
 * Implement callback for validation of form 'webform_exact_target_api_configure_form'
 */
function webform_exact_target_configure_form_validate($form, &$form_state) {
  $values = $form_state['values'];

  foreach ($values['mappings'] as $id => $val) {
    // Check that a source field was selected:

    if ($form['mappings'][$id]['#required'] && '0' == $val) {
      form_set_error($id, t('No source Webform field selected for required ExactTarget field %field.  Please select a Webform field from the dropdown.', array('%field' => $form['mappings'][$id]['#title'])));
    }
  }
}

/**
 * Implement callback for submit handling of form 'webform_exact_target_api_configure_form'
 */
function webform_exact_target_configure_form_submit($form, &$form_state) {
  $node = node_load($form_state['values']['nid'], $form_state['values']['vid']);
  $mappings = $form_state['values'];

  foreach ($mappings['mappings'] AS $key => $value) {
    if ($value == WFET_CREATE_NEW_FIELD) {
      $mappings['mappings'][$key] = webform_exact_target_create_new_field($form['#attribute_specs'][$key]);
    }
  }

  // Unwind list fieldsets into flat list

  $mappings['listid'] = array();
  foreach ($form_state['values']['wfet_list_groups'] as $type => $elems) {
    $lists = $elems['listid'];
    foreach ($lists as $key => $val) {
      if ($key == $val) {
        $mappings['listid'][] = $val;
      }
    }
  }

  _webform_exact_target_set_mappings($node, $mappings);
  drupal_set_message(t('Webform ExactTarget settings successfully saved.'), 'status');
}

/**
 * create a new field for the attributes
 *
 * This function currently does nothing, it still needs to be implemented.
 * TODO
 *
 * @param $specs
 *  Object containing the attribute information
 * @return
 *  string with the new field machine name
 */
function webform_exact_target_create_new_field($specs) {

}
