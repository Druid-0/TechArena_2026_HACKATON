# Post-by-post state and Java-branch audit

## S2 — first regular order then cancellation, second regular order then cheaper rebooking

* Step 6: `PreserveOtherServiceImpl.preserve` creates a new regular order. Assurance is zero, station food type nonzero, consignee fields empty. In `ts-preserve-other-service/.../PreserveOtherServiceImpl.java`, the branches start at lines 182, 196 and 218 respectively.
* Step 8: `InsidePaymentServiceImpl.pay` reads that order and selects the regular-order URL (lines 53–57). Internal account balance suffices, so its local-payment branch runs (lines 99–131). The just-created order moves from NOTPAID to PAID.
* Step 9: `CancelServiceImpl.calculateRefund` tries high-speed lookup before the regular-order lookup (lines 159, 177). The regular order has been paid, so it runs the nonzero refund calculation (lines 180–187).
* Step 10: `CancelServiceImpl.cancelOrder` similarly tries both order families (lines 48, 104). The regular cancellation succeeds and calls `drawbackMoney` (lines 115–127). The first local order is now CANCELLED; the baseline record is not overwritten.
* Step 11: a **second new order** is booked; it is distinct from the cancelled first order. Step 13 pays that second order.
* Step 14: `RebookServiceImpl.rebook` accepts the paid order (lines 50–80), checks the future travel date (lines 265–291), sees the lower price, invokes `drawBackMoney`, then `updateOrder` (lines 124–131). Same-family rebooking updates in place rather than delete-and-recreate (lines 220–235). Final new-order statuses: `CANCELLED`, `CHANGED`; one initial-refund event and one cheaper-difference-refund event.

## S3 — two independent high-speed orders

* Step 7: `PreserveServiceImpl.preserve` creates order 1 with assurance, station food and a 0.5 kg consignment. See condition guards at lines 179, 193, 218 of `ts-preserve-service/.../PreserveServiceImpl.java`.
* Step 8: a **second** order is created without optional additions; the `if` bodies for food, assurance and consignment do not run on this request. The scenario-wide graph still contains those branches because the first request executed them once.
* Steps 12 and 13: `InsidePaymentServiceImpl.pay` selects the high-speed order service (lines 53–55); each order has NOTPAID status before its own payment. Payments happen separately: after the first, local state is `[PAID, RESERVED]`; after the second, `[PAID, PAID]`.
* Step 14: `ConsignPriceServiceImpl.getPriceByWeightAndRegion` fetches the price configuration; the 0.5 kg weight exercises the initial-weight branch (lines 26–36).
* Step 15: assurance type listing is a separate route.

The state ledger models **which branch is reached**. It does not invent the unprovided UUIDs or trace runtime SQL, and the submission uses the scenario-wide union of edges, not per-step multiplicities. Source paths and line numbers refer to the user-supplied project.
