/*
 * Copyright (c) 2009 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/machintr.h>
#include <sys/systm.h>

#include <machine/pmap.h>
#include <machine/smp.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <machine_base/apic/mpapic.h>

#include "acpi_sdt.h"
#include "acpi_sdt_var.h"
#include "acpi_sci_var.h"

#define FADT_VPRINTF(fmt, arg...) \
do { \
	if (bootverbose) \
		kprintf("ACPI FADT: " fmt , ##arg); \
} while (0)

/* Fixed ACPI Description Table */
struct acpi_fadt {
	struct acpi_sdth	fadt_hdr;
	uint32_t		fadt_fw_ctrl;
	uint32_t		fadt_dsdt;
	uint8_t			fadt_rsvd1;
	uint8_t			fadt_pm_prof;
	uint16_t		fadt_sci_int;
	uint32_t		fadt_smi_cmd;
	uint8_t			fadt_acpi_en;
	uint8_t			fadt_acpi_dis;
	uint8_t			fadt_s4bios;
	uint8_t			fadt_pstate;
	/* More ... */
} __packed;

struct acpi_sci_mode {
	enum intr_trigger	sci_trig;
	enum intr_polarity	sci_pola;
};

static int			acpi_sci_irq = -1;
static enum intr_trigger	acpi_sci_trig = INTR_TRIGGER_CONFORM;
static enum intr_polarity	acpi_sci_pola = INTR_POLARITY_CONFORM;

static const struct acpi_sci_mode acpi_sci_modes[] = {
	/*
	 * NOTE: Order is critical
	 */
	{ INTR_TRIGGER_LEVEL,	INTR_POLARITY_LOW },
	{ INTR_TRIGGER_LEVEL,	INTR_POLARITY_HIGH },
	{ INTR_TRIGGER_EDGE,	INTR_POLARITY_HIGH },
	{ INTR_TRIGGER_EDGE,	INTR_POLARITY_LOW },

	/* Required last entry */
	{ INTR_TRIGGER_CONFORM,	INTR_POLARITY_CONFORM }
};

static void
fadt_probe(void)
{
	struct acpi_fadt *fadt;
	vm_paddr_t fadt_paddr;
	enum intr_trigger trig;
	enum intr_polarity pola;
	int enabled = 1;
	char *env;

	fadt_paddr = sdt_search(ACPI_FADT_SIG);
	if (fadt_paddr == 0) {
		kprintf("fadt_probe: can't locate FADT\n");
		return;
	}

	fadt = sdt_sdth_map(fadt_paddr);
	KKASSERT(fadt != NULL);

	/*
	 * FADT in ACPI specification 1.0 - 4.0
	 */
	if (fadt->fadt_hdr.sdth_rev < 1 || fadt->fadt_hdr.sdth_rev > 4) {
		kprintf("fadt_probe: unsupported FADT revision %d\n",
			fadt->fadt_hdr.sdth_rev);
		goto back;
	}

	if (fadt->fadt_hdr.sdth_len < sizeof(*fadt)) {
		kprintf("fadt_probe: invalid FADT length %u\n",
			fadt->fadt_hdr.sdth_len);
		goto back;
	}

	kgetenv_int("hw.acpi.sci.enabled", &enabled);
	if (!enabled)
		goto back;

	acpi_sci_irq = fadt->fadt_sci_int;

	env = kgetenv("hw.acpi.sci.trigger");
	if (env == NULL)
		goto back;

	trig = INTR_TRIGGER_CONFORM;
	if (strcmp(env, "edge") == 0)
		trig = INTR_TRIGGER_EDGE;
	else if (strcmp(env, "level") == 0)
		trig = INTR_TRIGGER_LEVEL;

	kfreeenv(env);

	if (trig == INTR_TRIGGER_CONFORM)
		goto back;

	env = kgetenv("hw.acpi.sci.polarity");
	if (env == NULL)
		goto back;

	pola = INTR_POLARITY_CONFORM;
	if (strcmp(env, "high") == 0)
		pola = INTR_POLARITY_HIGH;
	else if (strcmp(env, "low") == 0)
		pola = INTR_POLARITY_LOW;

	kfreeenv(env);

	if (pola == INTR_POLARITY_CONFORM)
		goto back;

	acpi_sci_trig = trig;
	acpi_sci_pola = pola;
back:
	if (acpi_sci_irq >= 0) {
		FADT_VPRINTF("SCI irq %d, %s/%s\n", acpi_sci_irq,
			     intr_str_trigger(acpi_sci_trig),
			     intr_str_polarity(acpi_sci_pola));
	} else {
		FADT_VPRINTF("SCI is disabled\n");
	}
	sdt_sdth_unmap(&fadt->fadt_hdr);
}
SYSINIT(fadt_probe, SI_BOOT2_PRESMP, SI_ORDER_SECOND, fadt_probe, 0);

static void
acpi_sci_dummy_intr(void *dummy __unused, void *frame __unused)
{
}

void
acpi_sci_config(void)
{
	const struct acpi_sci_mode *mode;

	if (acpi_sci_irq < 0)
		return;

	if (acpi_sci_trig != INTR_TRIGGER_CONFORM) {
		KKASSERT(acpi_sci_pola != INTR_POLARITY_CONFORM);
		machintr_intr_config(acpi_sci_irq,
		    acpi_sci_trig, acpi_sci_pola);
		return;
	}

	kprintf("ACPI FADT: SCI testing interrupt mode ...\n");
	for (mode = acpi_sci_modes; mode->sci_trig != INTR_TRIGGER_CONFORM;
	     ++mode) {
		void *sci_desc;
		long last_cnt;

		FADT_VPRINTF("SCI testing %s/%s\n",
		    intr_str_trigger(mode->sci_trig),
		    intr_str_polarity(mode->sci_pola));

		last_cnt = get_interrupt_counter(acpi_sci_irq);

		machintr_intr_config(acpi_sci_irq,
		    mode->sci_trig, mode->sci_pola);

		sci_desc = register_int(acpi_sci_irq,
		    acpi_sci_dummy_intr, NULL, "sci", NULL,
		    INTR_EXCL | INTR_CLOCK |
		    INTR_NOPOLL | INTR_MPSAFE | INTR_NOENTROPY);

		DELAY(100 * 1000);

		unregister_int(sci_desc);

		if (get_interrupt_counter(acpi_sci_irq) - last_cnt < 20) {
			acpi_sci_trig = mode->sci_trig;
			acpi_sci_pola = mode->sci_pola;

			kprintf("ACPI FADT: SCI select %s/%s\n",
			    intr_str_trigger(acpi_sci_trig),
			    intr_str_polarity(acpi_sci_pola));
			return;
		}
	}

	kprintf("ACPI FADT: no suitable interrupt mode for SCI, disable\n");
	acpi_sci_irq = -1;
}

int
acpi_sci_enabled(void)
{
	if (acpi_sci_irq >= 0)
		return 1;
	else
		return 0;
}

int
acpi_sci_pci_shariable(void)
{
	if (acpi_sci_irq >= 0 &&
	    acpi_sci_trig == INTR_TRIGGER_LEVEL &&
	    acpi_sci_pola == INTR_POLARITY_LOW)
		return 1;
	else
		return 0;
}
