#
# Copyright (C) 2024-2026 Texas Instruments Incorporated - https://www.ti.com/
#
# SPDX-License-Identifier: BSD-3-Clause
#

#
# Standalone build rules for the AM62L LPM WKUP SRAM stub.
#
# The stub is compiled and linked independently into a flat binary, then
# converted to a linkable .o blob via objcopy and added to BL31 as rodata.
#

LPM_STUB_DIR      := ${PLAT_PATH}/common/drivers/lpm
LPM_STUB_BUILD    := ${BUILD_PLAT}/lpm_stub
LPM_STUB_ELF      := ${LPM_STUB_BUILD}/lpm_stub.elf
LPM_STUB_BIN      := ${LPM_STUB_BUILD}/lpm_stub.bin
LPM_STUB_BLOB_O   := ${BUILD_PLAT}/bl31/lpm_stub_blob.o
LPM_STUB_LD_S     := ${LPM_STUB_DIR}/lpm_stub.ld.S
LPM_STUB_LD       := ${LPM_STUB_BUILD}/lpm_stub.ld

# Source files compiled into the standalone stub binary.
# switch_stack.S and k3_lpm_ctrl.c stay in BL31.
LPM_STUB_SOURCES  := \
	${LPM_STUB_DIR}/lpm_resume.S        \
	${LPM_STUB_DIR}/lpm_stub.c          \
	${LPM_STUB_DIR}/lpm_psc_raw.c       \
	${LPM_STUB_DIR}/lpm_pll_16fft_raw.c \
	${LPM_STUB_DIR}/lpm_ddr.c           \
	${LPM_STUB_DIR}/lpm_timeout.c       \
	${LPM_STUB_DIR}/lpm_trace.c         \
	${PLAT_PATH}/common/drivers/ti/uart/aarch64/16550_console.S        \
	lib/aarch64/cache_helpers.S         \
	lib/aarch64/misc_helpers.S

LPM_STUB_OBJS := $(addprefix ${LPM_STUB_BUILD}/,\
	$(patsubst %.c,%.o,$(patsubst %.S,%.o,$(notdir ${LPM_STUB_SOURCES}))))

LPM_STUB_CFLAGS    = ${TF_CFLAGS} -DIMAGE_LPM_STUB

LPM_STUB_ASFLAGS   = ${ASFLAGS}

LPM_STUB_LDFLAGS = $(filter-out -flto% -fuse-linker-plugin,${TF_LDFLAGS})

${LPM_STUB_LD}: ${LPM_STUB_LD_S} | ${LPM_STUB_BUILD}/
	$(s)echo "  CPP     $<"
	$(q)${$(ARCH)-cpp} -E -P -x assembler-with-cpp  ${TF_CFLAGS} $< -o $@

${LPM_STUB_BUILD}/%.o: ${LPM_STUB_DIR}/%.c | ${LPM_STUB_BUILD}/
	$(s)echo "  CC [stub]  $<"
	$(q)${$(ARCH)-cc} ${LPM_STUB_CFLAGS} -c $< -o $@

${LPM_STUB_BUILD}/%.o: ${LPM_STUB_DIR}/%.S | ${LPM_STUB_BUILD}/
	$(s)echo "  AS [stub]  $<"
	$(q)${$(ARCH)-as} -x assembler-with-cpp \
		${LPM_STUB_CFLAGS} ${LPM_STUB_ASFLAGS} -c $< -o $@

${LPM_STUB_BUILD}/16550_console.o: drivers/ti/uart/aarch64/16550_console.S | ${LPM_STUB_BUILD}/
	$(s)echo "  AS [stub]  $<"
	$(q)${$(ARCH)-as} -x assembler-with-cpp \
		${LPM_STUB_CFLAGS} ${LPM_STUB_ASFLAGS} -c $< -o $@

${LPM_STUB_BUILD}/%.o: lib/aarch64/%.S | ${LPM_STUB_BUILD}/
	$(s)echo "  AS [stub]  $<"
	$(q)${$(ARCH)-as} -x assembler-with-cpp \
		${LPM_STUB_CFLAGS} ${LPM_STUB_ASFLAGS} -c $< -o $@

${LPM_STUB_ELF}: ${LPM_STUB_OBJS} ${LPM_STUB_LD} | ${LPM_STUB_BUILD}/
	$(s)echo "  LD [stub]  $@"
	$(q)${$(ARCH)-ld} -o $@ \
		-T ${LPM_STUB_LD} \
		${LPM_STUB_LDFLAGS} \
		${LPM_STUB_OBJS}

${LPM_STUB_BIN}: ${LPM_STUB_ELF}
	$(s)echo "  BIN [stub] $@"
	$(q)${$(ARCH)-oc} -O binary $< $@

# objcopy -I binary derives symbol names from the input filename.  cd-ing into
# the build directory and passing the bare filename produces the short names
# _binary_lpm_stub_bin_{start,end,size} that k3_lpm_ctrl.c references.
${LPM_STUB_BLOB_O}: ${LPM_STUB_BIN} | $$(@D)/
	$(s)echo "  BLOB    $@"
	$(q)cd ${LPM_STUB_BUILD} && ${$(ARCH)-oc}		\
		-I binary -O elf64-littleaarch64 -B aarch64	\
		--rename-section .data=.rodata.lpm_stub		\
		$(notdir ${LPM_STUB_BIN}) ${LPM_STUB_BLOB_O}

${LPM_STUB_BUILD}/:
	$(q)mkdir -p $@
