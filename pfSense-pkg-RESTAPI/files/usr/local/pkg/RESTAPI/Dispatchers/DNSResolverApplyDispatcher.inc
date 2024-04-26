<?php

namespace RESTAPI\Dispatchers;

use RESTAPI\Core\Dispatcher;

class DNSResolverApplyDispatcher extends Dispatcher {
    # Only allow one Unbound reload to occur at a time
    public int $max_concurrency = 1;

    /**
     * Reloads the DNS Resolver and associated services.
     */
    protected function _process(mixed ...$arguments): void {
        $reload_unbound = 0;
        $reload_unbound |= services_unbound_configure();
        if ($reload_unbound === 0) {
            system_resolvconf_generate();
            system_dhcpleases_configure();
            clear_subsystem_dirty('unbound');
        }
    }
}