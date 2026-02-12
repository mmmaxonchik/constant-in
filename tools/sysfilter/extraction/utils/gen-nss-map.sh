#!/bin/bash

set -u

CFLAGS="-Wall -O2"
NSS_DEF_SRC="nss-def.c"
NSS_DEF="nss-def"

SORT_OPT="-u"
READELF_OPT="-W --dyn-sym"
TAIL_OPT="-n+4"
CUT_OPT_PREF="-d_ -f4-"
CUT_OPT_SUFF="-d@ -f1"
RM_OPT="-f"

AWK_FILTER='{ if ($7 != "UND") print $8 }'

# FIXME
NSS_LIBS_PATH="/lib/x86_64-linux-gnu"

# FIXME
NSS_LIBS="libnss_files.so.2 libnss_db.so.2 libnss_dns.so.2"
NSS_LIBS="$NSS_LIBS libnss_nis.so.2 libnss_compat.so.2"
NSS_LIBS="$NSS_LIBS libnss_mdns4_minimal.so.2 libnss_mdns4.so.2"
NSS_LIBS="$NSS_LIBS libnss_mdns6_minimal.so.2 libnss_mdns6.so.2"
NSS_LIBS="$NSS_LIBS libnss_mdns_minimal.so.2 libnss_mdns.so.2"
NSS_LIBS="$NSS_LIBS libnss_nisplus.so.2 libnss_hesiod.so.2"

# NSS_LIBS="$NSS_LIBS libnss_resolve.so.2 libnss_systemd.so.2"
# NSS_LIBS="$NSS_LIBS libnss_ldap.so.2 libnss_cache.so.2"
# NSS_LIBS="$NSS_LIBS libnss_docker.so.2 libnss_libvirt.so.2"
# NSS_LIBS="$NSS_LIBS libnss_myhostname.so.2 libnss_mymachines.so.2"
# NSS_LIBS="$NSS_LIBS libnss_winbind.so.2 libnss_wins.so.2"
# NSS_LIBS="$NSS_LIBS libnss_unknown.so.2 libnss_gw_name.so.2 libnss_sss.so.2"

NSS_PREFIX="^_nss_"

TMP_FNAME="tmp.lst"
NSS_LST_FNAME="nss.lst"
NSS_DEFMAP_FNAME="nss-def.map"

UNKNOWN_DB="undef"

[[ -e $NSS_DEF_SRC ]] && rm $RM_OPT $NSS_DEF_SRC
cat << __EOF > $NSS_DEF_SRC
/* adapted from nsswitch.c */
#include <stdio.h>
#include <stdlib.h>

#define DEFINE_ENT(h,nm)		\
	{ #h, "get"#nm"ent_r" },	\
	{ #h, "end"#nm"ent" },		\
	{ #h, "set"#nm"ent" },
#define DEFINE_GET(h,nm)		\
	{ #h, "get"#nm"_r" },
#define DEFINE_GETBY(h,nm,ky)		\
	{ #h, "get"#nm"by"#ky"_r" },

struct nss_map {
	const char *db;
	const char *func;
} tbl[] = {

	DEFINE_ENT (aliases, alias)
	DEFINE_GETBY (aliases, alias, name)

	DEFINE_ENT (ethers, ether)

	DEFINE_ENT (group, gr)
	DEFINE_GET (group, grgid)
	DEFINE_GET (group, grnam)

	DEFINE_ENT (hosts, host)
	DEFINE_GETBY (hosts, host, addr)
	DEFINE_GETBY (hosts, host, name)
	DEFINE_GETBY (hosts, host, name2)
	DEFINE_GET (hosts, hostton)
	DEFINE_GET (hosts, ntohost)
	DEFINE_GETBY (hosts, host, addr)
	DEFINE_GETBY (hosts, host, name)
	DEFINE_GETBY (hosts, host, name2)

	DEFINE_ENT (netgroup, netgr)

	DEFINE_ENT (networks, net)
	DEFINE_GETBY (networks, net, name)
	DEFINE_GETBY (networks, net, addr)
	DEFINE_GETBY (networks, net, name)
	DEFINE_GETBY (networks, net, addr)

	DEFINE_ENT (protocols, proto)
	DEFINE_GETBY (protocols, proto, name)
	DEFINE_GETBY (protocols, proto, number)

	DEFINE_ENT (passwd, pw)
	DEFINE_GET (passwd, pwnam)
	DEFINE_GET (passwd, pwuid)

	DEFINE_ENT (rpc, rpc)
	DEFINE_GETBY (rpc, rpc, name)
	DEFINE_GETBY (rpc, rpc, number)

	DEFINE_ENT (services, serv)
	DEFINE_GETBY (services, serv, name)
	DEFINE_GETBY (services, serv, port)

	DEFINE_ENT (shadow, sp)
	DEFINE_GET (shadow, spnam)

	/* additional mappings */
	{"ethers", "parse_etherent"},
	{"group", "setgrent_path"},
	{"gshadow", "getsgent_r"},
	{"gshadow", "getsgnam_r"},
	{"gshadow", "endsgent"},
	{"gshadow", "setsgent"},
	{"hosts", "gethostbyaddr2_r"},
	{"hosts", "gethostbyname3_r"},
	{"hosts", "gethostbyname4_r"},
	{"hosts", "guest_gethostbyname2_r"},
	{"hosts", "guest_gethostbyname3_r"},
	{"hosts", "guest_gethostbyname4_r"},
	{"hosts", "guest_gethostbyname_r"},
	{"hosts", "minimal_gethostbyaddr_r"},
	{"hosts", "minimal_gethostbyname2_r"},
	{"hosts", "minimal_gethostbyname3_r"},
	{"hosts", "minimal_gethostbyname4_r"},
	{"hosts", "minimal_gethostbyname_r"},
	{"hosts", "name_gethostbyname2_r"},
	{"hosts", "name_gethostbyname_r"},
	{"initgroups", "initgroups_dyn"},
	{"netgroup", "getcanonname_r"},
	{"publickey", "getpublickey"},
	{"publickey", "getsecretkey"},
	{"rpc", "netname2user"},
	{"networks", "parse_netent"},
	{"passwd", "setpwent_path"},
	{"protocols", "parse_protoent"},
	{"rpc", "parse_rpcent"},
	{"services", "parse_servent"},
	{"shadow", "setspent_path"},
};

int
main(int argc, char **argv)
{
	for (int i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++)
		fprintf(stdout, "%s\t%s\n", tbl[i].func, tbl[i].db);

	return EXIT_SUCCESS;
}
__EOF

cc $CFLAGS -o $NSS_DEF $NSS_DEF_SRC
[[ $? -ne 0 ]] && exit 1
./$NSS_DEF > $NSS_DEFMAP_FNAME

[[ -e $TMP_FNAME ]] && rm $RM_OPT $TMP_FNAME
for lib in $NSS_LIBS; do
	readelf $READELF_OPT $NSS_LIBS_PATH/$lib | tail $TAIL_OPT | awk "$AWK_FILTER" | egrep $NSS_PREFIX | cut $CUT_OPT_PREF | cut $CUT_OPT_SUFF >> $TMP_FNAME
	[[ $? -ne 0 ]] && exit 1
done

cat $TMP_FNAME | sort $SORT_OPT > $NSS_LST_FNAME
rm $RM_OPT $TMP_FNAME

for func in $(cat $NSS_LST_FNAME); do
	egrep $func $NSS_DEFMAP_FNAME > /dev/null
	[[ $? -ne 0 ]] && echo -e "$func\t$UNKNOWN_DB" >> $TMP_FNAME
done

cat $NSS_DEFMAP_FNAME $TMP_FNAME | sort $SORT_OPT
rm $RM_OPT $TMP_FNAME $NSS_LST_FNAME $NSS_DEFMAP_FNAME $NSS_DEF $NSS_DEF_SRC
