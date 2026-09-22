global-incdirs-y += .
srcs-y += main.c
# rng.c hardcodes the g12b (A311D) RNG register's physical address; other flavors (e.g. axg) are a
# different chip family and would need their own address if they ever enable CFG_WITH_SOFTWARE_PRNG=n.
srcs-$(PLATFORM_FLAVOR_g12b) += rng.c
srcs-$(PLATFORM_FLAVOR_g12b) += huk.c
