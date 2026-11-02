# Basic MMU Initialization Sequences

> Arm 官方原始说明（已转为 Markdown）。原文见 `upstream-ReadMe.md`。

## Description

This folder contains an assortment of different MMU initialization examples.
These are intended as a base from which to build a reproducer when working on
cases.

They are **not** intended as fully functional examples.

Each folder contains a `startup.S` file, that implements the MMU setup described
in the folder title.

The examples target the Base Platform AEM (FVP_Base_AEMv8A-ARMv8-A), with the
following parameters:

```text
-C bp.secure_memory=0 -C cluster0.NUM_CORES=1 -C cluster1.NUM_CORES=0
```

> Note: Running on a different platform, or with different parameters, will
> require some degree of porting.

The E2H example additionally requires:

```text
-C cluster0.has_arm_v8-1=1
```

The AF update example additionally requires:

```text
-C cluster0.has_arm_v8-1=1 -C cluster0.hardware_translation_table_update_implemented=1
```

For the "el3_AF_full_mem_map" example, the parameters are:

```text
-C cluster0.NUM_CORES=1 -C cluster1.NUM_CORES=0
```

## Limitations

The examples use the GAS `.fill` directive to allocate the space for the tables,
specifying they will be filled with 0s. When loaded via a debugger, this removes
the need to zero the memory at run-time, making the examples much quicker to
run. However, in a "real" system you would want a run-time zeroing loop
(included but commented out).

## File list

```text
<Example>
  |
  |-> el1_stage1
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el1_stage1_and_stage2
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el2_stage1_l1andl2
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el2_stage1_l1only
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el2_stage1_l1only_e2h
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el3_AF_update   (ARMv8.1-A and later)
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el3_AF_full_mem_map
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el3_stage1_l1andl2
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el3_stage1_l1only
  |   |-> Makefile
  |   |-> startup.S
  |
  |-> el3_stage1_l0_to_l2
  |   |-> Makefile
  |   |-> startup.S
```
