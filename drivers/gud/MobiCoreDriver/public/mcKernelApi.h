#ifndef _MOBICORE_KERNELMODULE_API_H_
#define _MOBICORE_KERNELMODULE_API_H_

struct mcInstance;

struct mcInstance *mobicore_open(
	void
);
int mobicore_release(
	struct mcInstance	*pInstance
);
int mobicore_allocate_wsm(
	struct mcInstance	*pInstance,
	unsigned long		requestedSize,
	uint32_t		*pHandle,
	void			**pKernelVirtAddr,
	void			**pPhysAddr
);
int mobicore_free(
	struct mcInstance	*pInstance,
	uint32_t		handle
);
int mobicore_map_vmem(
	struct mcInstance	*pInstance,
	void			*addr,
	uint32_t		len,
	uint32_t		*pHandle,
	void			**physWsmL2Table
);
int mobicore_unmap_vmem(
	struct mcInstance	*pInstance,
	uint32_t		handle
);
#endif /* _MOBICORE_KERNELMODULE_API_H_ */
