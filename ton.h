#ifndef TON_H
#define TON_H
#include <UserCAPI.h>
#include <stdbool.h>

// Forward declaration of GetMSTick, if it's not globally visible
// extern uint32_t GetMSTick(void);

typedef struct
{
    uint32_t PT;
    uint32_t ET;
    uint32_t startTime;
    bool Q;
} T_TON;

typedef struct
{
    uint32_t PT;
    uint32_t ET;
    uint32_t startTime;
    bool Q;
} T_TOF;

/* ---------------------------------------------------------------------------
 *  TON – Timer ON Delay
 *
 *  in          : Eingangssignal (true → Timer starten)
 *  presetTime  : voreingestellte Zeit in Millisekunden
 *
 *  Rückgabe    : true, wenn die voreingestellte Zeit abgelaufen ist,
 *                sonst false.
 *
 *  Verhalten:
 *      • Beim ersten true‑Puls wird der Timer gestartet.
 *      • Während in==true wird ET (elapsed time) hochgezählt.
 *      • Sobald ET >= PT wird Q auf true gesetzt.
 *      • Fällt in==false oder presetTime==0, wird alles zurückgesetzt.
 * --------------------------------------------------------------------------- */
bool TON(T_TON *t, bool in, uint32_t presetTime)
{
    if (!in || presetTime == 0U)
    {
        /* Reset – Timer aus, keine Ablauffrist */
        t->ET = 0U;
        t->startTime = 0U;
        t->Q = false;
    }
    else
    {
        /* Timer starten, falls er noch nicht lief */
        if (t->startTime == 0U)
        {
            t->PT = presetTime;
            t->startTime = GetMSTick();
        }

        /* Laufzeit ermitteln */
        t->ET = GetMSTick() - t->startTime;

        /* Ausgang setzen */
        t->Q = (t->ET >= t->PT);
    }
    return t->Q;
}

/* ---------------------------------------------------------------------------
 *  TOF – Timer OFF Delay
 *
 *  in          : Eingangssignal (true → Timer zurücksetzen)
 *  presetTime  : voreingestellte Zeit in Millisekunden
 *
 *  Rückgabe    : true, solange die OFF‑Delay‑Zeit noch nicht abgelaufen ist,
 *                sonst false.
 *
 *  Verhalten:
 *      • Solange in==true wird Q permanent auf true gehalten
 *        und der interne Zähler zurückgesetzt.
 *      • Fällt in von true nach false, startet der Off‑Delay‑Timer.
 *      • Während ET < PT bleibt Q==true; danach wird Q false.
 * --------------------------------------------------------------------------- */
bool TOF(T_TOF *t, bool in, uint32_t presetTime)
{
    if (in)
    {
        /* Eingang aktiv → sofort zurücksetzen */
        t->ET = 0U;
        t->startTime = 0U;
        t->Q = true;
    }
    else
    {
        /* Eingang ist false → Off‑Delay starten (falls nötig) */
        if (t->startTime == 0U)
        {
            t->PT = presetTime;
            t->startTime = GetMSTick();
        }

        t->ET = GetMSTick() - t->startTime;

        /* Q bleibt true, solange die Wartezeit noch nicht verstrichen ist */
        t->Q = (t->ET < t->PT);
    }
    return t->Q;
}

#define TON_RESET(x) TON(x, false, 0)
#define TOF_RESET(x) TOF(x, true, 0)

#endif // TON_H
