#ifndef _BACKPORT_LINUX_PCI_H
#define _BACKPORT_LINUX_PCI_H
#include_next <linux/pci.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,4,0)
#include <linux/pci-aspm.h>
#endif

#if defined(CONFIG_PCI)
#if LINUX_VERSION_IS_LESS(5,3,0)
static inline int
backport_pci_disable_link_state(struct pci_dev *pdev, int state)
{
	u16 aspmc;

	pci_disable_link_state(pdev, state);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspmc);
	if ((state & PCIE_LINK_STATE_L0S) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L0S))
		return -EPERM;

	if ((state & PCIE_LINK_STATE_L1) &&
	    (aspmc & PCI_EXP_LNKCTL_ASPM_L1))
		return -EPERM;

	return 0;
}
#define pci_disable_link_state LINUX_BACKPORT(pci_disable_link_state)

#endif /* < 5.3 */
#endif /* defined(CONFIG_PCI) */

/*
 * Provide stubs for functions that are inside #ifdef CONFIG_PCI in the kernel
 * headers but used by ath12k on AHB platforms where CONFIG_PCI is not set.
 * All these code paths are only reached for PCI-attached devices; on AHB-only
 * boards these stubs are dead code but must be compilable.
 */
#ifndef CONFIG_PCI
#ifndef CONFIG_PCIE_QCOM
static inline int pcie_set_link_speed(struct pci_dev *dev, u16 speed)
{
	return -ENODEV;
}
static inline int pcie_set_link_width(struct pci_dev *dev, u16 width)
{
	return -ENODEV;
}
#endif /* !CONFIG_PCIE_QCOM */

static inline int pcie_capability_read_word(struct pci_dev *dev, int pos,
					    u16 *val)
{
	*val = 0;
	return -ENODEV;
}
static inline int pcie_capability_clear_and_set_word(struct pci_dev *dev,
						     int pos, u16 clear, u16 set)
{
	return -ENODEV;
}
static inline int pcie_capability_clear_word(struct pci_dev *dev, int pos,
					     u16 clear)
{
	return -ENODEV;
}
static inline int pci_is_enabled(struct pci_dev *pdev) { return 0; }
static inline void pci_free_irq_vectors(struct pci_dev *dev) {}
static inline int pci_request_region(struct pci_dev *dev, int bar,
				     const char *res_name)
{
	return -ENODEV;
}
static inline void pci_release_region(struct pci_dev *dev, int bar) {}
static inline void pci_stop_and_remove_bus_device_locked(struct pci_dev *dev) {}
static inline void pci_lock_rescan_remove(void) {}
static inline void pci_unlock_rescan_remove(void) {}
static inline unsigned int pci_rescan_bus(struct pci_bus *bus) { return 0; }

#endif /* !CONFIG_PCI */

#endif /* _BACKPORT_LINUX_PCI_H */
