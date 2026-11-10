/* CoF's nine-byte ScoreInfo carries per-player equipment flags, not kills.
 * Verified with Ghidra/ReVa: server 10015640; client 10069b60/1001ef90.
 * Bit 0x20 controls the co-op vest spotlight. FlashFlags is independent.
 * Keep this network snapshot until another message or CL_ClearState; the
 * server sends updates on changes, not once per frame.
 */
#ifndef COF_PLAYER_LIGHTS_H
#define COF_PLAYER_LIGHTS_H

#define COF_PLAYER_LIGHT_SLOTS 65
#define COF_PLAYER_LIGHT_KNOWN 0x10000
#define COF_PLAYER_LIGHT_VEST 0x20
typedef struct cof_player_lights_s
{
    unsigned short flags[COF_PLAYER_LIGHT_SLOTS];
    unsigned char known[COF_PLAYER_LIGHT_SLOTS];
} cof_player_lights_t;

static inline int CofPlayerLightsScoreInfo(cof_player_lights_t *state,
    int size, const void *buffer)
{
    const unsigned char *bytes = (const unsigned char *)buffer;
    unsigned int index;
    if (!state || !bytes || size != 9) return 0;
    index = bytes[0];
    if (index == 0 || index >= COF_PLAYER_LIGHT_SLOTS) return 0;
    state->flags[index] = (unsigned short)(bytes[1] | (bytes[2] << 8));
    state->known[index] = 1;
    return 1;
}

static inline int CofPlayerLightsGet(const cof_player_lights_t *state, int index)
{
    if (!state || index <= 0 || index >= COF_PLAYER_LIGHT_SLOTS || !state->known[index]) return 0;
    return COF_PLAYER_LIGHT_KNOWN | state->flags[index];
}
#endif
