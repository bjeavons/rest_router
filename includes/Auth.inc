<?php

/**
 * @file
 * Authentication plugin
 */

abstract class RestRouterAuth {

  /**
   * Currently processed request
   *
   * @var RestRouterRequest
   */
  protected $request;

  /**
   * Plugin settings array
   */
  protected $settings;

  /**
   * Constructor
   *
   * @param $request
   *   RestRouterRequest that is being processed
   */
  function __construct(RestRouterRequest $request, $settings) {
    $this->request = $request;
    $this->settings = $settings;
  }

  abstract public function authenticate();
}

/**
 * OAuth authentication support for RestRouter
 *
 * @param $settings
 *   'type' => '2legged' or '3legged'
 *   'context' => Drupal OAuth context
 */
class RestRouterAuthOAuth extends RestRouterAuth {
  public function authenticate() {
    $cred = $this->settings['type'];

    // If no credentials are needed we'll pass this one through
    if ($cred == 'none') {
      return FALSE;
    }

    try {
      module_load_include('inc', 'oauth_common');

      list($signed, $consumer, $token) = oauth_common_verify_request();

      if (!$signed && ($cred == '2legged' || $cred == '3legged')) {
        throw new OAuthException('The request must be signed');
      }
      if ($consumer == NULL) {
        throw new OAuthException('Missing consumer token');
      }
      if ($consumer->context !== $this->settings['context']) {
        throw new OAuthException('The consumer is not valid in the current context');
      }

      // Validate the token, if it's required by the method
      if ($cred == '3legged') {
        if (empty($token->key)) {
          throw new OAuthException('Missing access token');
        }
        if (!$token->authorized) {
          throw new OAuthException('The access token is not authorized');
        }
      }

      // Load the user if the request was authenticated using a token
      // that's associated with a account.
      if ($cred == '3legged') {
        if ($token->uid) {
          return user_load($token->uid);
        }
      }
      else if ($cred == '2legged') {
        if ($consumer->uid) {
          // This authenticates as the user who owns 'key';  It is for 2-stage 
          // OAuth and is vastly inferior to 3-stage OAuth.
          return user_load($consumer->uid);
        }
      }
    }
    catch (OAuthException $e) {
      drupal_add_http_header('WWW-Authenticate', sprintf('OAuth realm="%s"', url('', array('absolute' => TRUE))));
    }
  }
}

/**
 * Class RestRouterAuthAnonymous
 *
 * Anonymous authentication for RestRouter.
 */
class RestRouterAuthAnonymous extends RestRouterAuth {
  public function authenticate() {
    return user_load(0);
  }
}

/**
 * Class RestRouterAuthBasic
 *
 * Basic Auth support for RestRouter.
 *
 * Client includes an Authorization: Basic header with a base64 encoded
 * username:password . So for user foo with password bar, the header would be:
 *
 * Authorization: Basic Zm9vOmJhcg==
 *
 * Note that if this request is not sent over a secure connection the username
 * and password will be visible.
 */
class RestRouterAuthBasic extends RestRouterAuth {
  public function authenticate() {
    if (empty($_SERVER['PHP_AUTH_USER']) || empty($_SERVER['PHP_AUTH_PW'])) {
      return FALSE;
    }
    $username = $_SERVER['PHP_AUTH_USER'];
    $password = $_SERVER['PHP_AUTH_PW'];
    $account = user_load_by_name($username);
    if (!$account) {
      return FALSE;
    }

    require_once DRUPAL_ROOT . '/' . variable_get('password_inc', 'includes/password.inc');
    if (user_check_password($password, $account)) {
      return $account;
    }
    else {
      return FALSE;
    }
  }
}
