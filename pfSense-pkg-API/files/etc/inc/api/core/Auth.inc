<?php
//   Copyright 2023 Jared Hendrickson
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

namespace API\Core;

require_once("api/core/Tools.inc");

use API;

# Creates an object capable of verifying authentication and authorization based on the configuration
class Auth {
    private $api_config;
    public $auth_mode;
    public $read_mode;
    public $req_privs;
    public $username;
    public $privs;
    public $ip_address;
    public $is_authenticated;
    public $is_authorized;

    # Create our method constructor
    public function __construct($req_privs=[], $enforce_auth_mode=null, $read_mode=null){
        $this->api_config = API\Core\Tools\get_api_config()[1];
        $this->auth_mode = (is_null($enforce_auth_mode)) ? $this->api_config["authmode"] : $enforce_auth_mode;
        $this->read_mode = (is_null($read_mode)) ? array_key_exists("readonly", $this->api_config) : $read_mode;
        $this->req_privs = $req_privs;
        $this->privs = [];
        $this->ip_address = $_SERVER["REMOTE_ADDR"];
        $this->is_authenticated = $this->authenticate();
        $this->is_authorized = $this->authorize();
    }

    # AUTHENTICATION #
    # Attempts to authenticate using local database authentication
    private function authenticate_local_database() {
        # Extract the username and password from basic authentication
        $this->username = $_SERVER['PHP_AUTH_USER'];
        $client_token = $_SERVER['PHP_AUTH_PW'];

        # Authenticate against local database
        if (authenticate_user($this->username, $client_token)) {
            # Ensure user is not disabled
            if (API\Core\Tools\is_user_disabled($this->username) === false) {
                unset($_SESSION["Username"]);
                $_SESSION["Username"] = $this->username;
                return true;
            }
        }
        return false;
    }

    # Attempts to authenticate using JWT authentication
    private function authenticate_jwt() {
        # Extract token type and JWT from authorization header
        $auth_header = explode(" ", $_SERVER["HTTP_AUTHORIZATION"]);
        $token_type = $auth_header[0];
        $token = $auth_header[1];
        $decoded_jwt = API\Core\Tools\decode_jwt($token);

        # Check that the JWT from our Authorization header is valid
        if ($token_type === "Bearer" and $decoded_jwt !== false) {
            # Extract the owner's username from the JWT's payload data
            $this->username = $decoded_jwt["data"];

            # Ensure user is not disabled
            if (API\Core\Tools\is_user_disabled($this->username) === false) {
                unset($_SESSION["Username"]);
                $_SESSION["Username"] = $this->username;
                return true;
            }
        }
        return false;
    }

    # Attempts to authenticate using API token authentication
    private function authenticate_token() {
        # Extract client ID and token from the authorization header
        $client_id = explode(" ", $_SERVER["HTTP_AUTHORIZATION"])[0];
        $client_token = explode(" ", $_SERVER["HTTP_AUTHORIZATION"])[1];

        # Check if authentication is successful with this client ID and token
        if (API\Core\Tools\authenticate_token($client_id, $client_token) === true) {
            $this->username = pack("H*", $client_id);
            # Ensure user is not disabled
            if (API\Core\Tools\is_user_disabled($this->username) === false) {
                unset($_SESSION["Username"]);
                $_SESSION["Username"] = $this->username;
                return true;
            }
        }
        return false;
    }

    # Chooses correct auth method based on configured auth mode. Returns bool.
    public function authenticate() {
        # Attempt to authenticate
        if ($this->auth_mode === "local") {
            $resp = $this->authenticate_local_database();
        }
        elseif ($this->auth_mode === "jwt") {
            $resp = $this->authenticate_jwt();
        }
        elseif ($this->auth_mode === "token") {
            $resp = $this->authenticate_token();
        }
        else {
            $resp = false;
        }

        # Set our object is_authenticated attribute to our authentication resp, log auth, and return the resp
        $this->is_authenticated = $resp;
        $this->__log_authentication($this->is_authenticated);
        return $this->is_authenticated;
    }

    # AUTHORIZATION #
    # Checks if the current user has the necessary privileges to access this resource
    public function authorize() {
        # Local variables
        $authorized = false;
        $client_config =& getUserEntry($this->username);;
        $this->privs = get_user_privileges($client_config);

        # If no require privileges were given, assume call is always authorized
        if (!empty($this->req_privs)) {
            # Check that client's IP is authorized
            if ($this->__is_ip_authorized()) {
                # If API is in readonly mode, only allow GET requests
                if (!$this->read_mode or ($this->read_mode and $_SERVER['REQUEST_METHOD'] === "GET")) {
                    # Loop through each of our req privs and ensure client has them, also check if access is read only
                    foreach ($this->req_privs as &$priv) {
                        if (in_array($priv, $this->privs)) {
                            $authorized = true;
                            break;
                        }
                    }
                }
            }
        } else {
            $authorized = true;
        }

        # Set our class is_authorized attribute to our authorization resp and return the resp
        $this->is_authorized = $authorized;
        return $authorized;
    }

    # Check if our client's IP is within our API access list
    private function __is_ip_authorized() {
        # Check if we have any API access list entries
        if (!empty($this->api_config["access_list"])) {
            # Check if the client's IP is within any of the allowed subnets
            foreach (explode(" ", $this->api_config["access_list"]) as $subnet) {
                # This IP is authorized if it's within this subnet
                if (API\Core\Tools\is_ip_in_cidr($this->ip_address, $subnet)) {
                    return true;
                }
            }
            # IP is not authorized if it's not within any configured subnet
            return false;
        }
        # The API access list is not configured, authorize any IP
        else {
            return true;
        }
    }

    # Logs the authentication attempt if login protection is enabled for the API
    private function __log_authentication($authenticated) {
        # Variables
        $username = ($this->username) ?: "unknown";
        $ip_address = $this->ip_address;

        # Log authentication attempts if enabled
        if (isset($this->api_config["enable_login_protection"])) {
            # Log failed authentication
            if (!$authenticated) {
                # This log entry is required for Login Protection to work, do not change the log text.
                log_auth(
                    gettext("webConfigurator authentication error for user '{$username}' from: {$ip_address}")
                );
            }
            # Log successful authentication if the API is configured to do so. Disabled by default to avoid log spam.
            elseif (isset($this->api_config["log_successful_auth"])) {
                log_auth(
                    gettext("Successful login for user '{$username}' from: {$ip_address} (Local Database)")
                );
            }
        }
    }
}