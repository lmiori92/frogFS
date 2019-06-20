# frogFS
FrogFS is a filesystem designed for simplicity and for very constrained physical storage (KB range) e.g. EEPROM. It supports fragmentation.

# Features
- Record is named after an index (no filename)
- Record Read
- Record Write
- Record Erase
- Fragmentation to reuse erased holes
- Design to run on small physical storage units (EEPROMs, NVRAMs, ...)
- Storage formatting
- In-RAM only allocation table

# Limitations
- 32kB data per record max.
- 127 data records max.
- The physical storage is entirely scanned at boot to build the allocation table

# Testing
In order to guarantee a degree of quality and to avoid data losses / corruption a set of module and integration tests have been developed.
