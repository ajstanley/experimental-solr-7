<?php

function ajax_example_simplest($form, &$form_state) {
  $form = array();
  $form['changethis'] = array(
    '#title' => t("Choose something and explain why"),
    '#type' => 'select',
    '#options' => array(
      'one' => 'one',
      'two' => 'two',
      'three' => 'three',
    ),
    '#ajax' => array(
      'callback' => 'ajax_example_simplest_callback',
      'wrapper' => 'replace_textfield_div',
     ),
  );


// This entire form element will be replaced with an updated value.
  // However, it has to have the prefix/suffix to work right, as the entire
  // div is replaced.
  // In this example, the description is dynamically updated during form
  // rebuild.


$form['replace_textfield'] = array(
    '#type' => 'textfield',
    '#title' => t("Why"),
    '#prefix' => '<div id="replace_textfield_div">',
    '#suffix' => '</div>',
  );

  if (!empty(
$form_state['values']['changethis'])) {
    $form['replace_textfield']['#description'] = t("Say why you chose") .  " '{$form_state['values']['changethis']}'";
  }
  return $form;
}

function ajax_example_simplest_callback($form, $form_state) {
  // The form has already been submitted and updated. We can return the replaced
  // item as it is.
  return $form['replace_textfield'];
}
?>
