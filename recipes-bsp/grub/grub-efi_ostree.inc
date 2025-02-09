#
# Copyright (C) 2016-2017 Wind River Systems, Inc.
#

FILESEXTRAPATHS:prepend := "${THISDIR}/grub-efi:"

SRC_URI += " \
    file://0001-Make-UEFI-watchdog-behaviour-configurable.patch \
    file://0001-correct-grub_errno-if-allocate-memory-at-preferred-a.patch \
    file://0001-grub-verify-Add-skip_check_cfg-variable.patch \
    file://grub-runtime.cfg \
"

EFI_BOOT_PATH = "/boot/efi/EFI/BOOT"

GRUB_BUILDIN += "tftp efinet reboot"
GRUB_BUILDIN += "${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', 'chain regexp efivar', '', d)}"

do_compile:append:class-native() {
        make grub-editenv
}

skip_check_grub_cfg() {
    if [ "${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', '1', '0', d)}" = "1" -a \
         "${@bb.utils.contains('DISTRO_FEATURES', 'lat', '1', '0', d)}" = "1" ]; then
        cat<<EOF>>${WORKDIR}/cfg
set skip_check_cfg=1
EOF
    fi
}
do_compile[postfuncs] += "skip_check_grub_cfg"

do_install:append:class-native() {
    install -m 0755 grub-editenv "${D}${bindir}"
}

do_install:append:class-target() {
    install -d ${D}${EFI_BOOT_PATH}
    install -m 0600 "${WORKDIR}/grub-runtime.cfg" "${D}${EFI_BOOT_PATH}/grub.cfg"
    sed -i "s#%DISTRO_NAME%#${DISTRO_NAME}#g" "${D}${EFI_BOOT_PATH}/grub.cfg"
    sed -i "s#%DISTRO_VERSION%#${DISTRO_VERSION}#g" "${D}${EFI_BOOT_PATH}/grub.cfg"
    echo -n "password_pbkdf2 ${OSTREE_GRUB_USER} " > ${WORKDIR}/pw
    cat "${OSTREE_GRUB_PW_FILE}" >> ${WORKDIR}/pw
    sed -i "s#%OSTREE_GRUB_USER%#${OSTREE_GRUB_USER}#g" "${D}${EFI_BOOT_PATH}/grub.cfg"
    sed -i "s#%OSTREE_GRUB_PW%#$(cat ${WORKDIR}/pw)#g" "${D}${EFI_BOOT_PATH}/grub.cfg"
    sed -i "s#%OSTREE_CONSOLE%#${OSTREE_CONSOLE}#g" "${D}${EFI_BOOT_PATH}/grub.cfg"
    if [ "${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', '1', '0', d)}" = "0" ]; then
        sed -i '/^get_efivar/,/^fi/d' "${D}${EFI_BOOT_PATH}/grub.cfg"
    fi

    efi_target=`echo ${GRUB_IMAGE} | sed 's/^grub-efi-//'`
    if [ "${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', '1', '0', d)}" = "0" ]; then
        install -m 644 ${B}/${GRUB_IMAGE_PREFIX}${GRUB_IMAGE} ${D}${EFI_BOOT_PATH}/$efi_target
    fi

    cat<<EOF>${WORKDIR}/cfg_nosecure
search.file (\$cmdpath)/EFI/BOOT/grub.cfg root
set prefix=(\$root)/EFI/BOOT
EOF

    grub-mkimage -c ../cfg_nosecure -p "/EFI/BOOT" -d "./grub-core" \
        -O "${GRUB_TARGET}-efi" -o "${B}/bootx64-nosig.efi" \
        ${GRUB_BUILDIN}

    install -m 0644 "${B}/bootx64-nosig.efi" "${D}${EFI_BOOT_PATH}/bootx64-nosig.efi"
}

do_deploy:append:class-target() {
    install -d ${DEPLOYDIR}

    if [ "${@bb.utils.contains('DISTRO_FEATURES', 'efi-secure-boot', '1', '0', d)}" = "1" ]; then
        if [ -e ${D}${EFI_BOOT_PATH}/grub.cfg.p7b ] ; then
            install -m 0600 "${D}${EFI_BOOT_PATH}/grub.cfg.p7b" "${DEPLOYDIR}"
        fi
        if [ -e ${D}${EFI_BOOT_PATH}/grub.cfg.sig ] ; then
            install -m 0600 "${D}${EFI_BOOT_PATH}/grub.cfg.sig" "${DEPLOYDIR}"
        fi
    fi
    install -m 0600 "${D}${EFI_BOOT_PATH}/grub.cfg" "${DEPLOYDIR}"
    install -m 0644 "${D}${EFI_BOOT_PATH}/bootx64-nosig.efi" "${DEPLOYDIR}"
}

FILES:${PN} += "${EFI_BOOT_PATH}"

addtask  deploy after do_install before do_package

