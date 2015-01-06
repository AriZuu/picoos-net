DIR_MODINC += $(MOD) $(MOD)/drivers
ifeq '$(strip $(NETCFG_STACK))' '6'
CDEFINES += NETSTACK_CONF_WITH_IPV4=0
CDEFINES += NETSTACK_CONF_WITH_IPV6=1 UIP_CONF_IPV6=1
else
ifeq '$(strip $(NETCFG_STACK))' '4'
CDEFINES += NETSTACK_CONF_WITH_IPV4=1
CDEFINES += NETSTACK_CONF_WITH_IPV6=0 UIP_CONF_IPV6=0
else
$(error Network stack must be selected by setting NETCFG_STACK to 4 or 6)
endif
endif

