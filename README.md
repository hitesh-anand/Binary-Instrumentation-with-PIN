# Binary-Instrumentation-with-PIN
This repository contains my submissions for the assignments in the course CS422: Computer Architecture, offered during Fall, 2023 by Prof. Mainak Chaudhuri


## Task 1

* Categorize the x86 instructions according to their type in one of the following categories:
  1. NOPs				(XED_CATEGORY_NOP)
  2. Direct calls			(XED_CATEGORY_CALL)
  3. Indirect calls		(XED_CATEGORY_CALL)
  4. Returns            		(XED_CATEGORY_RET)
  5. Unconditional branches 	(XED_CATEGORY_UNCOND_BR)
  6. Conditional branches 	(XED_CATEGORY_COND_BR)
  7. Logical operations		(XED_CATEGORY_LOGICAL)
  8. Rotate and shift     	(XED_CATEGORY_ROTATE | XED_CATEGORY_SHIFT)
  9. Flag operations		(XED_CATEGORY_FLAGOP)
  10. Vector instructions		(XED_CATEGORY_AVX | XED_CATEGORY_AVX2 | XED_CATEGORY_AVX2GATHER | XED_CATEGORY_AVX512)
  11. Conditional moves		(XED_CATEGORY_CMOV)
  12. MMX and SSE instructions	(XED_CATEGORY_MMX | XED_CATEGORY_SSE)
  13. System calls		(XED_CATEGORY_SYSCALL)
  14. Floating-point		(XED_CATEGORY_X87_ALU)
  15. Others: whatever is left
