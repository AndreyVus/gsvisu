/* pagemanager.c
 * Komplettes Beispiel in reinem C14 ohne Zeitfeld in TouchEvent
 */

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

/* ---- Hardware / OS Stubs ---- */
typedef enum { TOUCH_DOWN, TOUCH_ON, TOUCH_UP } TouchType;

typedef struct {
    TouchType type;
    int x, y;
} TouchEvent;

int GetMSTick(void) {
    /* Stub – hier echte Zeitquelle */
    static int fake = 0;
    return fake += 16; /* ~60Hz */
}

void ContainerOnPos(int frame, int x, int y) {
    printf("ContainerOnPos(frame=%d, x=%d, y=%d)\n", frame, x, y);
}

void ContainerMove(int frame, int x, int y) {
    printf("ContainerMove(frame=%d, x=%d, y=%d)\n", frame, x, y);
}

/* ---- Gesture ---- */
typedef struct {
    int tx, ty;       /* Sollrichtung */
    float tolCos;     /* cos-Toleranz */
    int minDist;
    int maxDuration;
} GestureDef;

typedef struct {
    bool valid;
    int dx, dy;
} GestureResult;

typedef struct {
    GestureDef def;
    bool active;
    int startX, startY;
    int startTime;
    GestureResult lastResult;
    bool hasResult;
} GestureRecognizer;

void gesture_onTouchEvent(GestureRecognizer* gr, const TouchEvent* e) {
    int now = GetMSTick();

    if (e->type == TOUCH_DOWN) {
        gr->active = true;
        gr->startX = e->x;
        gr->startY = e->y;
        gr->startTime = now;
        gr->hasResult = false;
    } else if ((e->type == TOUCH_UP || e->type == TOUCH_ON) && gr->active) {
        int dt = now - gr->startTime;
        int dx = e->x - gr->startX;
        int dy = e->y - gr->startY;
        float len = sqrtf((float)(dx * dx + dy * dy));
        float refLen = sqrtf((float)(gr->def.tx * gr->def.tx + gr->def.ty * gr->def.ty));
        float dot = dx * gr->def.tx + dy * gr->def.ty;
        float cosA = (len > 0 && refLen > 0) ? dot / (len * refLen) : 0;

        if (len >= gr->def.minDist && dt <= gr->def.maxDuration && cosA >= gr->def.tolCos) {
            gr->lastResult.valid = true;
            gr->lastResult.dx = dx;
            gr->lastResult.dy = dy;
            gr->hasResult = true;
        }
        gr->active = false;
    }
}

void gesture_updateTimeout(GestureRecognizer* gr, int now) {
    if (gr->active && (now - gr->startTime > gr->def.maxDuration)) {
        gr->lastResult.valid = false;
        gr->hasResult = false;
        gr->active = false;
    }
}

/* ---- Frames ---- */
typedef struct {
    void (*init)(void);
    void (*onEnter)(void);
    void (*cycle)(int c, TouchEvent* e);
    void (*onExit)(void);
} Frame;

/* ---- PageManager ---- */
typedef enum { ANIM_IDLE, ANIM_RUNNING } AnimState;

typedef struct {
    Frame* frames;
    int framesAnzahl;
    int curFrame;
    int targetFrame;

    GestureRecognizer grLeft;
    GestureRecognizer grRight;
    GestureRecognizer grUp;
    GestureRecognizer grDown;

    int width, height;
    int xOffset;
    int duration;
    int startTime;
    AnimState state;

    int startX_cur, targetX_cur;
    int startY_cur, targetY_cur;
    int startX_new, targetX_new;
    int startY_new, targetY_new;
    bool dualAnim;
} PageManager;

/* ---- Animation ---- */
static void PageManager_startAnim(PageManager* pm,
                                  int startXc, int targetXc,
                                  int startYc, int targetYc,
                                  int startXn, int targetXn,
                                  int startYn, int targetYn,
                                  bool dual) {
    pm->startTime = GetMSTick();
    pm->startX_cur = startXc; pm->targetX_cur = targetXc;
    pm->startY_cur = startYc; pm->targetY_cur = targetYc;
    pm->startX_new = startXn; pm->targetX_new = targetXn;
    pm->startY_new = startYn; pm->targetY_new = targetYn;
    pm->dualAnim = dual;
    pm->state = ANIM_RUNNING;
}

static void PageManager_updateAnim(PageManager* pm) {
    int now = GetMSTick();
    int elapsed = now - pm->startTime;
    if (elapsed >= pm->duration) {
        ContainerOnPos(pm->curFrame, pm->targetX_cur, pm->targetY_cur);
        if (pm->dualAnim) {
            ContainerOnPos(pm->targetFrame, pm->targetX_new, pm->targetY_new);
            pm->curFrame = pm->targetFrame;
        }
        pm->state = ANIM_IDLE;
        return;
    }
    float t = (float)elapsed / pm->duration;
    int xCur = pm->startX_cur + (int)((pm->targetX_cur - pm->startX_cur) * t);
    int yCur = pm->startY_cur + (int)((pm->targetY_cur - pm->startY_cur) * t);
    ContainerMove(pm->curFrame, xCur, yCur);
    if (pm->dualAnim) {
        int xNew = pm->startX_new + (int)((pm->targetX_new - pm->startX_new) * t);
        int yNew = pm->startY_new + (int)((pm->targetY_new - pm->startY_new) * t);
        ContainerMove(pm->targetFrame, xNew, yNew);
    }
}

/* ---- StateMachine ---- */
void PageManager_stateMachine(PageManager* pm) {
    if (pm->state == ANIM_RUNNING) {
        PageManager_updateAnim(pm);
        return;
    }
    if (pm->grLeft.hasResult && pm->grLeft.lastResult.valid) {
        pm->grLeft.hasResult = false;
        PageManager_startAnim(pm, 0, -pm->xOffset, 0, 0, 0, 0, 0, 0, false);
    } else if (pm->grRight.hasResult && pm->grRight.lastResult.valid) {
        pm->grRight.hasResult = false;
        PageManager_startAnim(pm, -pm->xOffset, 0, 0, 0, 0, 0, 0, 0, false);
    } else if (pm->grUp.hasResult && pm->grUp.lastResult.valid) {
        pm->grUp.hasResult = false;
        pm->targetFrame = (pm->curFrame + 1) % pm->framesAnzahl;
        if (pm->frames[pm->curFrame].onExit) pm->frames[pm->curFrame].onExit();
        if (pm->frames[pm->targetFrame].onEnter) pm->frames[pm->targetFrame].onEnter();
        ContainerOnPos(pm->targetFrame, 0, pm->height);
        PageManager_startAnim(pm, 0, 0, 0, -pm->height, 0, 0, pm->height, 0, true);
    } else if (pm->grDown.hasResult && pm->grDown.lastResult.valid) {
        pm->grDown.hasResult = false;
        pm->targetFrame = (pm->curFrame - 1 + pm->framesAnzahl) % pm->framesAnzahl;
        if (pm->frames[pm->curFrame].onExit) pm->frames[pm->curFrame].onExit();
        if (pm->frames[pm->targetFrame].onEnter) pm->frames[pm->targetFrame].onEnter();
        ContainerOnPos(pm->targetFrame, 0, -pm->height);
        PageManager_startAnim(pm, 0, 0, 0, pm->height, 0, 0, -pm->height, 0, true);
    }
}

/* ---- Cycle ---- */
void PageManager_cycle(PageManager* pm, int c, TouchEvent* e) {
    for (; c-- > 0; ++e) {
        if (e->type == TOUCH_DOWN || e->type == TOUCH_ON || e->type == TOUCH_UP) {
            if (pm->state == ANIM_IDLE) {
                gesture_onTouchEvent(&pm->grLeft, e);
                gesture_onTouchEvent(&pm->grRight, e);
                gesture_onTouchEvent(&pm->grUp, e);
                gesture_onTouchEvent(&pm->grDown, e);
            }
        }
    }
    int now = GetMSTick();
    if (pm->state == ANIM_IDLE) {
        gesture_updateTimeout(&pm->grLeft, now);
        gesture_updateTimeout(&pm->grRight, now);
        gesture_updateTimeout(&pm->grUp, now);
        gesture_updateTimeout(&pm->grDown, now);
    }
    PageManager_stateMachine(pm);
    if (pm->frames[pm->curFrame].cycle) {
        pm->frames[pm->curFrame].cycle(c, e);
    }
}

/* ---- Beispiel Frames ---- */
void page0_cycle(int c, TouchEvent* e) { printf("page0_cycle\n"); }
void page1_cycle(int c, TouchEvent* e) { printf("page1_cycle\n"); }

Frame frames[] = {
    { NULL, NULL, page0_cycle, NULL },
    { NULL, NULL, page1_cycle, NULL }
};

int main(void) {
    PageManager pm = {0};
    pm.frames = frames;
    pm.framesAnzahl = 2;
    pm.curFrame = 0;
    pm.width = 1024;
    pm.height = 600;
    pm.xOffset = 150;
    pm.duration = 500;
    pm.state = ANIM_IDLE;

    /* Gesten initialisieren */
    pm.grLeft.def = (GestureDef){ -1, 0, 0.9f, 50, 500 };
    pm.grRight.def = (GestureDef){ 1, 0, 0.9f, 50, 500 };
    pm.grUp.def = (GestureDef){ 0, -1, 0.9f, 50, 500 };
    pm.grDown.def = (GestureDef){ 0, 1, 0.9f, 50, 500 };

    /* Dummy Event-Sequenz */
    TouchEvent seq[] = {
        { TOUCH_DOWN, 100, 100 },
        { TOUCH_UP, 100, 0 }
    };
    PageManager_cycle(&pm, 2, seq);
    for (int i = 0; i < 40; i++) {
        PageManager_cycle(&pm, 0, NULL);
    }
    return 0;
}