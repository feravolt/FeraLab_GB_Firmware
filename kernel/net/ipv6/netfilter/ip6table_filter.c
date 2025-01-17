/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2000-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("ip6tables filter table");

#define FILTER_VALID_HOOKS ((1 << NF_INET_LOCAL_IN) | \
			    (1 << NF_INET_FORWARD) | \
			    (1 << NF_INET_LOCAL_OUT))

static struct
{
	struct ip6t_replace repl;
	struct ip6t_standard entries[3];
	struct ip6t_error term;
} initial_table __net_initdata = {
	.repl = {
		.name = "filter",
		.valid_hooks = FILTER_VALID_HOOKS,
		.num_entries = 4,
		.size = sizeof(struct ip6t_standard) * 3 + sizeof(struct ip6t_error),
		.hook_entry = {
			[NF_INET_LOCAL_IN] = 0,
			[NF_INET_FORWARD] = sizeof(struct ip6t_standard),
			[NF_INET_LOCAL_OUT] = sizeof(struct ip6t_standard) * 2
		},
		.underflow = {
			[NF_INET_LOCAL_IN] = 0,
			[NF_INET_FORWARD] = sizeof(struct ip6t_standard),
			[NF_INET_LOCAL_OUT] = sizeof(struct ip6t_standard) * 2
		},
	},
	.entries = {
		IP6T_STANDARD_INIT(NF_ACCEPT),	/* LOCAL_IN */
		IP6T_STANDARD_INIT(NF_ACCEPT),	/* FORWARD */
		IP6T_STANDARD_INIT(NF_ACCEPT),	/* LOCAL_OUT */
	},
	.term = IP6T_ERROR_INIT,		/* ERROR */
};

static struct xt_table packet_filter = {
	.name		= "filter",
	.valid_hooks	= FILTER_VALID_HOOKS,
	.lock		= __RW_LOCK_UNLOCKED(packet_filter.lock),
	.me		= THIS_MODULE,
	.af		= AF_INET6,
};

/* The work comes in here from netfilter.c. */
static unsigned int
ip6t_in_hook(unsigned int hook,
		   struct sk_buff *skb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn)(struct sk_buff *))
{
	return ip6t_do_table(skb, hook, in, out,
			     dev_net(in)->ipv6.ip6table_filter);
}

static unsigned int
ip6t_local_out_hook(unsigned int hook,
		   struct sk_buff *skb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn)(struct sk_buff *))
{
#if 0
	/* root is playing with raw sockets. */
	if (skb->len < sizeof(struct iphdr)
	    || ip_hdrlen(skb) < sizeof(struct iphdr)) {
		if (net_ratelimit())
			printk("ip6t_hook: happy cracking.\n");
		return NF_ACCEPT;
	}
#endif

	return ip6t_do_table(skb, hook, in, out,
			     dev_net(out)->ipv6.ip6table_filter);
}

static struct nf_hook_ops ip6t_ops[] __read_mostly = {
	{
		.hook		= ip6t_in_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP6_PRI_FILTER,
	},
	{
		.hook		= ip6t_in_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET6,
		.hooknum	= NF_INET_FORWARD,
		.priority	= NF_IP6_PRI_FILTER,
	},
	{
		.hook		= ip6t_local_out_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_FILTER,
	},
};

/* Default to forward because I got too much mail already. */
static bool forward = true;
module_param(forward, bool, 0000);

static int __net_init ip6table_filter_net_init(struct net *net)
{
	/* Register table */
	net->ipv6.ip6table_filter =
		ip6t_register_table(net, &packet_filter, &initial_table.repl);
	if (IS_ERR(net->ipv6.ip6table_filter))
		return PTR_ERR(net->ipv6.ip6table_filter);
	return 0;
}

static void __net_exit ip6table_filter_net_exit(struct net *net)
{
	ip6t_unregister_table(net->ipv6.ip6table_filter);
}

static struct pernet_operations ip6table_filter_net_ops = {
	.init = ip6table_filter_net_init,
	.exit = ip6table_filter_net_exit,
};

static int __init ip6table_filter_init(void)
{
	int ret;


	/* Entry 1 is the FORWARD hook */
	initial_table.entries[1].target.verdict = forward? -2:-1;

	ret = register_pernet_subsys(&ip6table_filter_net_ops);
	if (ret < 0)
		return ret;

	/* Register hooks */
	ret = nf_register_hooks(ip6t_ops, ARRAY_SIZE(ip6t_ops));
	if (ret < 0)
		goto cleanup_table;

	return ret;

 cleanup_table:
	unregister_pernet_subsys(&ip6table_filter_net_ops);
	return ret;
}

static void __exit ip6table_filter_fini(void)
{
	nf_unregister_hooks(ip6t_ops, ARRAY_SIZE(ip6t_ops));
	unregister_pernet_subsys(&ip6table_filter_net_ops);
}

module_init(ip6table_filter_init);
module_exit(ip6table_filter_fini);
