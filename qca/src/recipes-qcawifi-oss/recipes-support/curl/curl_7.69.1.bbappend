PACKAGECONFIG ??= "${@bb.utils.filter('DISTRO_FEATURES', 'ipv6', d)} libidn proxy threaded-resolver verbose zlib ssl nghttp2"

EXTRA_OECONF = "--enable-libcurl-option \
                --enable-crypto-auth \
                --with-ca-bundle=${sysconfdir}/ssl/certs/ca-certificates.crt \
                --with-nghttp2=${STAGING_DIR_TARGET} \
               "
