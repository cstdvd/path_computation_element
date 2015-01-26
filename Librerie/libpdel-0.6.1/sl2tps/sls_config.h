
/*
 * @COPYRIGHT@
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 */

#ifndef _SL2TPS_CONFIG_H_
#define _SL2TPS_CONFIG_H_

/* User account info */
struct sls_user {
	char			*name;
	char			*password;
	struct in_addr		ip;
};

/* Array of users */
DEFINE_STRUCTS_ARRAY(sls_users_ary, struct sls_user);

/* Array of IP addresses */
DEFINE_STRUCTS_ARRAY(ip_addr_ary, struct in_addr);

/* Configuration object for the sls application */
struct sls_config {
	char			*pidfile;
	struct alog_config	error_log;
	struct in_addr		bind_ip;
	u_int16_t		bind_port;
	struct in_addr		inside_ip;
	struct ip_addr_ary	dns_servers;
	struct ip_addr_ary	nbns_servers;
	struct in_addr		ip_pool_start;
	u_int			ip_pool_size;
	struct sls_users_ary	users;
};

/* Variables */
extern const	struct structs_type sls_config_type;
extern struct	app_config_ctx *sls_config_ctx;
extern const	struct sls_config *const sls_curconf;

/* Functions */
extern int	sls_config_init(struct pevent_ctx *ctx, const char *path);

#endif	/* !_SL2TPS_CONFIG_H_ */

