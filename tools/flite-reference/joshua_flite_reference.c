/* Host-only reference for issue #6. Not shipped, not part of the CPU runtime.
 * Synthesizes text with unmodified CMU Flite 2.2 and the cmu_us_rms voice,
 * writes a RIFF WAV and dumps per-segment timing and F0 for golden tests.
 *
 * Usage: joshua_flite_reference "text" out.wav out-segments.tsv
 * Copyright (c) 2026 JOSHUA contributors. MIT License (see LICENSE).
 * Links against Flite, Copyright (c) 1999-2017 Carnegie Mellon University;
 * see THIRD_PARTY_NOTICES.md. */
#include <stdio.h>
#include "flite.h"

#ifdef _WIN32
/* cst_file.h defines these as C99 inline functions; one translation unit must
 * provide the external definitions. */
extern int c99_vsnprintf(char *str, size_t size, const char *format, va_list ap);
extern int c99_snprintf(char *str, size_t size, const char *format, ...);
#endif

cst_voice *register_cmu_us_rms(const char *voxdir);
void usenglish_init(cst_voice *v);
cst_lexicon *cmulex_init(void);

int main(int argc, char **argv)
{
    cst_voice *voice;
    cst_utterance *utterance;
    cst_item *segment;
    FILE *dump;

    if (argc != 4) {
        fprintf(stderr, "usage: %s text out.wav segments.tsv\n", argv[0]);
        return 2;
    }
    flite_init();
    flite_add_lang("eng", usenglish_init, cmulex_init);
    flite_add_lang("usenglish", usenglish_init, cmulex_init);
    voice = register_cmu_us_rms(NULL);
    if (voice == NULL)
        return 1;
    utterance = flite_synth_text(argv[1], voice);
    if (utterance == NULL)
        return 1;
    if (cst_wave_save_riff(utt_wave(utterance), argv[2]) != CST_OK_FORMAT)
        return 1;
    dump = fopen(argv[3], "w");
    if (dump == NULL)
        return 1;
    fprintf(dump, "segment\tend_s\tword\tstress\n");
    for (segment = relation_head(utt_relation(utterance, "Segment"));
         segment != NULL; segment = item_next(segment)) {
        fprintf(dump, "%s\t%.4f\t%s\t%s\n",
                item_feat_string(segment, "name"),
                item_feat_float(segment, "end"),
                ffeature_string(segment, "R:SylStructure.parent.parent.name"),
                ffeature_string(segment, "R:SylStructure.parent.stress"));
    }
    fclose(dump);
    delete_utterance(utterance);
    return 0;
}
