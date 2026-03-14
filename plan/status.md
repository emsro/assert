# Status

## Current phase: 2 — deep read of `asrtc` complete, awaiting human review

## Read progress

| Library | Files read | Issues filed | Deep read done |
|---------|-----------|--------------|----------------|
| asrtl | all | L01–L13 + L-cov1–6 | **yes** |
| asrtr | all (reactor.h, record.h, status.h, asrtr.c, asrtr_test.c, asrtr_tests.h) | R01–R04 + R-cov1–4 | **yes** |
| asrtc | all (controller.h/c, allocator.h/c, handlers.h, callbacks.h, result.h, status.h, default_*.h) | C01–C09 + C-cov1–6 | **yes** |
| test/ | asrtl_test.c, asrtr_test.c, asrtc_test.c | T01–T04 | asrtl + asrtr + asrtc |
