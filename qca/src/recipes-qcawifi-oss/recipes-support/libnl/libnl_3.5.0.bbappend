
addtask copy_files_sysroot after do_populate_sysroot before do_package
do_copy_files_sysroot() {
	install -d ${STAGING_DIR}/
	install -d ${STAGING_DIR}/libnl
	cp -fR ${AUTOTOOLS_AUXDIR}/include/* ${STAGING_DIR}/libnl
}
