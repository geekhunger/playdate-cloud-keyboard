#ifndef BRB_ELASTIC_MARKER_H
#define BRB_ELASTIC_MARKER_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Two spring-driven cap centers. Retargeting preserves position and velocity.
typedef struct {
    float left, right, left_velocity, right_velocity, target;
    uint32_t at;
    int direction, tag;
    bool active;
} BrbElasticMarker;

#define BRB_ELASTIC_SETTLE_POSITION 0.04f
#define BRB_ELASTIC_SETTLE_VELOCITY 0.5f

static inline bool brb_elastic_marker_settled(const BrbElasticMarker* marker,
    float left_target,float right_target) {
    return marker->active&&fabsf(marker->left-left_target)<BRB_ELASTIC_SETTLE_POSITION&&
        fabsf(marker->right-right_target)<BRB_ELASTIC_SETTLE_POSITION&&
        fabsf(marker->left_velocity)<BRB_ELASTIC_SETTLE_VELOCITY&&
        fabsf(marker->right_velocity)<BRB_ELASTIC_SETTLE_VELOCITY;
}

// Underdamped analytic step: omega > 0 and 0 < damping < 1.
static inline void brb_elastic_spring_configured(float* position, float* velocity,
    float target,float seconds,float omega,float damping,float minimum,float maximum,
    bool preserve_inward_velocity) {
    if (*position == target && *velocity == 0.0f) return;
    const float decay = damping * omega;
    const float frequency = omega * sqrtf(1.0f - damping * damping);
    const float offset = *position - target;
    const float b = (*velocity + decay * offset) / frequency;
    const float cosine = cosf(frequency * seconds), sine = sinf(frequency * seconds);
    const float envelope = expf(-decay * seconds);
    *position = target + envelope * (offset * cosine + b * sine);
    *velocity = envelope * ((*velocity) * cosine - (decay * b + frequency * offset) * sine);
    if (*position < minimum || *position > maximum) {
        const bool outward=(*position<minimum&&*velocity<0)||(*position>maximum&&*velocity>0);
        *position = fminf(maximum, fmaxf(minimum, *position));
        if(!preserve_inward_velocity||outward)*velocity = 0.0f;
    }
    if (fabsf(*position - target) < BRB_ELASTIC_SETTLE_POSITION &&
        fabsf(*velocity) < BRB_ELASTIC_SETTLE_VELOCITY) {
        *position = target;
        *velocity = 0.0f;
    }
}

// Existing consumers retain their exact spring and boundary behavior.
static inline void brb_elastic_spring(float* position,float* velocity,float target,
    float seconds,bool leading,float minimum,float maximum) {
    brb_elastic_spring_configured(position,velocity,target,seconds,
        leading?42.0f:25.0f,leading?0.78f:0.62f,minimum,maximum,false);
}

typedef struct {
    float head_omega,head_damping,tail_omega,tail_damping;
    float maximum_tail,entry_tail;
} BrbElasticMarkerMotion;

static inline void brb_elastic_marker_stretch_entry(BrbElasticMarker* marker,
    float target,float minimum,float maximum,float stretch) {
    const bool right=maximum-target>target-minimum;
    const float room=right?maximum-target:target-minimum;
    stretch=fminf(room,stretch);
    if(right){marker->right=fmaxf(marker->right,target+stretch);marker->direction=-1;}
    else {marker->left=fminf(marker->left,target-stretch);marker->direction=1;}
}

// Two independent caps naturally retain more horizontal trail as retargeting
// accelerates. The analytic spring returns exponentially, with bounded elastic
// overshoot. No velocity estimate, frame queue or additional marker state is
// needed. A coordinate rebase translates the existing marker before this call.
static inline void brb_elastic_marker_jelly_update(BrbElasticMarker* marker,int tag,
    float target,float pull,float minimum,float maximum,uint32_t now,
    const BrbElasticMarkerMotion* motion) {
    target=fminf(maximum,fmaxf(minimum,target));
    const uint32_t elapsed=now-marker->at;
    if(!marker->active||marker->tag!=tag||elapsed>250) {
        const bool entry=marker->tag!=0&&marker->tag!=tag&&elapsed<=250;
        *marker=(BrbElasticMarker){.left=target,.right=target,.target=target,
            .at=now,.tag=tag,.active=true,.direction=1};
        if(entry) {
            // Stretch into the new viewport, never across intervening groups.
            brb_elastic_marker_stretch_entry(marker,target,minimum,maximum,
                fminf(motion->entry_tail,motion->maximum_tail));
        }
        return;
    }
    if(target!=marker->target)marker->direction=target>marker->target?1:-1;
    const float seconds=(float)elapsed*0.001f;
    const float left_target=fminf(maximum,fmaxf(minimum,target+fminf(0,pull)));
    const float right_target=fminf(maximum,fmaxf(minimum,target+fmaxf(0,pull)));
    const float low=fmaxf(minimum,left_target-motion->maximum_tail);
    const float high=fminf(maximum,right_target+motion->maximum_tail);
    // Preserve same-time retarget continuity. On a real tick, discard only
    // velocity pointing out of a viewport/tail boundary, never inward motion.
    if(elapsed) {
        const bool left_head=marker->direction<0;
        brb_elastic_spring_configured(&marker->left,&marker->left_velocity,left_target,
            seconds,left_head?motion->head_omega:motion->tail_omega,
            left_head?motion->head_damping:motion->tail_damping,low,high,true);
        brb_elastic_spring_configured(&marker->right,&marker->right_velocity,right_target,
            seconds,left_head?motion->tail_omega:motion->head_omega,
            left_head?motion->tail_damping:motion->head_damping,low,high,true);
    }
    marker->target=target;marker->at=now;
}

static inline void brb_elastic_marker_update(BrbElasticMarker* marker, int tag,
                                             float target, float minimum,
                                             float maximum, uint32_t now) {
    target = fminf(maximum, fmaxf(minimum, target));
    const uint32_t elapsed = now - marker->at;
    if (!marker->active || marker->tag != tag || elapsed > 250) {
        *marker = (BrbElasticMarker){.left=target, .right=target, .target=target,
            .at=now, .tag=tag, .active=true, .direction=1};
        return;
    }
    const float seconds = (float)elapsed * 0.001f;
    brb_elastic_spring(&marker->left, &marker->left_velocity, marker->target,
        seconds, marker->direction < 0, minimum, maximum);
    brb_elastic_spring(&marker->right, &marker->right_velocity, marker->target,
        seconds, marker->direction > 0, minimum, maximum);
    if (target != marker->target) marker->direction = target > marker->target ? 1 : -1;
    marker->target = target;
    marker->at = now;
}

#endif
