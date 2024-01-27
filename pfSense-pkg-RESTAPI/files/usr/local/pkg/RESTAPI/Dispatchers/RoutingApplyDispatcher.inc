<?php

namespace RESTAPI\Dispatchers;

use RESTAPI\Core\Dispatcher;

class RoutingApplyDispatcher extends Dispatcher
{
    # Only allow one routing apply to occur at a time
    public int $max_concurrency = 1;

    /**
     * Reload routing services to apply routing changes.
     */
    public function process(...$arguments): void
    {
        global $g;

        # Check for the pending changes file and unserialize it
        if (file_exists("{$g['tmp_path']}/.system_routes.apply")) {
            $to_apply_list = unserialize(file_get_contents("{$g['tmp_path']}/.system_routes.apply"));

            # Run commands to apply these changes
            foreach ($to_apply_list as $to_apply) {
                mwexec("{$to_apply}");
            }

            @unlink("{$g['tmp_path']}/.system_routes.apply");
        }

        # Reload associated services
        system_routing_configure();
        system_resolvconf_generate();
        filter_configure();
        setup_gateways_monitor();
        send_event("service reload dyndnsall");
        clear_subsystem_dirty("staticroutes");
    }
}