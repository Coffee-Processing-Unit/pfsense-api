<?php

namespace API\Models;

use API\Core\Auth;
use API\Core\Model;
use API\Dispatchers\SystemHostnameApplyDispatcher;
use API\Fields\StringField;
use API\Validators\HostnameValidator;

class SystemHostname extends Model
{
    public StringField $hostname;
    public StringField $domain;

    public function __construct(
        mixed $id = null, mixed $parent_id = null, mixed $data = [], Auth $client = null, bool $async = true
    )
    {
        # Set model attributes
        $this->config_path = "system";
        $this->update_strategy = "merge";
        $this->always_apply = true;

        # Set model fields
        $this->hostname = new StringField(
            required: true,
            allow_empty: true,
            validators: [new HostnameValidator(allow_hostname: true, allow_domain: false)],
            help_text: "The hostname portion of the FQDN to assign to this system."
        );
        $this->domain = new StringField(
            required: true,
            allow_empty: true,
            validators: [new HostnameValidator(allow_hostname: false, allow_domain: true)],
            help_text: "The domain portion of the FQDN to assign to this system."
        );

        parent::__construct(id: $id, parent_id: $parent_id, data: $data, client: $client, async: $async);
    }

    /**
     * Apply this hostname to the system and restart associated services.
     */
    public function apply() {
        (new SystemHostnameApplyDispatcher(async: $this->async))->spawn_process();
    }
}