# M0 threat model and safety limits

This unsigned development image is not a secure general-purpose OS. Firmware,
the boot medium, and the image are trusted inputs. Malicious firmware, physical
access, DMA-capable devices, SMM, model execution, and hostile processes are not
contained by the prototype. No user processes or AI run at all.

Current safeguards: read-only boot workflow; no storage/network protocol use;
QEMU-only bounded serial polling and firmware retries; validated framebuffer arithmetic;
bounded memory-map buffer; independent stack and stop-only exception handlers;
separate PE code/data permissions; no untrusted command text execution.
Firmware enforcement of section permissions is not claimed to be universal.

Disabling Secure Boot reduces boot-chain protection. Do it only on the designated
test machine. The ISO is unsigned and must not be represented as trusted merely
because Ventoy starts it. Artifact hashes identify bytes but do not authenticate
their origin. A signature/trust/revocation design is deferred.

Never test on a production machine. The kernel can hang, reset, or expose firmware
bugs. A black screen is a failed test, not proof that a driver is working. Although
the owner considers the Latitude disk disposable, disk erasure and installation
are explicitly outside this implementation. Host tools write only project files.

M2 and later must add owned page tables, W^X/NX, stack guards, validated pointers,
fault diagnostics, physical frame accounting, and audited CPU mitigation policies.
M4 must introduce ring-3 isolation and authenticated handles before agents or
other untrusted software are allowed. No performance/security equivalence claims
against Linux can be made from this boot stub.
