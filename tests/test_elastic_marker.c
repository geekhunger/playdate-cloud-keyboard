#include "elastic_marker.h"
#include <assert.h>
#include <stdio.h>

static void check(const BrbElasticMarker* marker) {
    assert(isfinite(marker->left) && isfinite(marker->right));
    assert(isfinite(marker->left_velocity) && isfinite(marker->right_velocity));
    assert(marker->left >= 10 && marker->left <= 180);
    assert(marker->right >= 10 && marker->right <= 180);
}

static const BrbElasticMarkerMotion jelly={42,0.78f,19,0.64f,36,18};

static float jelly_trace(float pixels_per_second) {
    BrbElasticMarker marker={0};
    brb_elastic_marker_jelly_update(&marker,1,40,0,10,280,0,&jelly);
    float total=0;unsigned samples=0;
    const float period=18000/pixels_per_second;
    const uint32_t stop=(uint32_t)ceilf(8*period);
    uint32_t now=5;
    for(;now<=stop+5;now+=5) {
        const int steps=(int)fminf(8,floorf((float)now/period));
        const float target=40+18*(float)steps;
        brb_elastic_marker_jelly_update(&marker,1,target,0,10,280,now,&jelly);
        assert(marker.left>=10&&marker.right<=280);
        assert(target-marker.left<=jelly.maximum_tail+0.001f);
        if(now>period*3&&now<stop){total+=target-marker.left;++samples;}
    }
    const uint32_t stopped_at=now;
    bool bounced=false;
    while(now-stopped_at<650) {
        brb_elastic_marker_jelly_update(&marker,1,184,0,10,280,now,&jelly);
        if(marker.left>184.5f)bounced=true;
        if(brb_elastic_marker_settled(&marker,184,184))break;
        now+=5;
    }
    assert(bounced); // Horizontal elastic recovery, without scaling a glyph.
    assert(now-stopped_at<650&&marker.left==184&&marker.right==184);
    return total/(float)samples;
}

static void test_jelly(void) {
    const float normal=jelly_trace(233),fast=jelly_trace(466);
    assert(normal>10&&fast>normal*1.5f&&fast<=jelly.maximum_tail);
    BrbElasticMarker marker={0};
    brb_elastic_marker_jelly_update(&marker,1,30,0,10,180,0,&jelly);
    brb_elastic_marker_jelly_update(&marker,1,48,0,10,180,0,&jelly);
    assert(marker.left==30&&marker.right==30);
    brb_elastic_marker_jelly_update(&marker,1,48,0,10,180,20,&jelly);
    assert(marker.right>marker.left);
    const BrbElasticMarker before=marker;
    brb_elastic_marker_jelly_update(&marker,1,30,0,10,180,20,&jelly);
    assert(marker.left==before.left&&marker.right==before.right);
    assert(marker.left_velocity==before.left_velocity&&marker.right_velocity==before.right_velocity);
    marker.left_velocity=-12;marker.right_velocity=18;
    brb_elastic_marker_stretch_entry(&marker,30,10,180,18);
    assert(marker.left_velocity==-12&&marker.right_velocity==18);
    assert(marker.left<=before.left&&marker.right>=before.right);
    brb_elastic_marker_jelly_update(&marker,2,10,0,10,180,40,&jelly);
    assert(marker.left==10&&marker.right==28); // Inward new-group arrival.
    for(uint32_t now=60;now<700;now+=20)
        brb_elastic_marker_jelly_update(&marker,2,10,0,10,180,now,&jelly);
    assert(brb_elastic_marker_settled(&marker,10,10));
    brb_elastic_marker_jelly_update(&marker,3,180,0,10,180,700,&jelly);
    assert(marker.left==162&&marker.right==180);
    brb_elastic_marker_jelly_update(&marker,4,80,0,10,180,1000,&jelly);
    assert(marker.left==80&&marker.right==80); // Paused clock cannot fling.
    marker.active=false;
    brb_elastic_marker_jelly_update(&marker,4,100,0,10,180,1000,&jelly);
    assert(marker.left==100&&marker.right==100);
    // A cyclic endpoint may arrive after its old marker was hidden for an
    // entire row-follow. Initialization must precede the one-time entry shape.
    marker.active=false;
    brb_elastic_marker_jelly_update(&marker,4,180,0,10,180,1520,&jelly);
    brb_elastic_marker_stretch_entry(&marker,marker.target,10,180,jelly.entry_tail);
    assert(marker.left==162&&marker.right==180);
    for(uint32_t now=1540;now<=2160;now+=20)
        brb_elastic_marker_jelly_update(&marker,4,180,0,10,180,now,&jelly);
    assert(brb_elastic_marker_settled(&marker,180,180));
    for(uint32_t frame=0;frame<20000;++frame) {
        brb_elastic_marker_jelly_update(&marker,4,10+(float)((frame*73)%171),
            frame%2?6:-6,10,180,UINT32_MAX-200+frame*7,&jelly);
        check(&marker);
    }
    float position=-10,velocity=100;
    brb_elastic_spring_configured(&position,&velocity,100,0,19,0.64f,0,180,true);
    assert(position==0&&velocity==100); // Tail clamp preserves inward motion.
    position=-10;velocity=-100;
    brb_elastic_spring_configured(&position,&velocity,100,0,19,0.64f,0,180,true);
    assert(position==0&&velocity==0);
    BrbElasticMarker batch={.left=45,.right=50,.target=70,.at=0,.direction=1,.tag=1,.active=true};
    BrbElasticMarker split=batch;
    brb_elastic_marker_jelly_update(&batch,1,70,0,10,180,60,&jelly);
    for(uint32_t now=10;now<=60;now+=10)
        brb_elastic_marker_jelly_update(&split,1,70,0,10,180,now,&jelly);
    assert(fabsf(batch.left-split.left)<0.001f&&fabsf(batch.right-split.right)<0.001f);
}

int main(void) {
    BrbElasticMarker marker = {0};
    brb_elastic_marker_update(&marker, 1, 30, 10, 180, 0);
    assert(marker.left == 30 && marker.right == 30);
    brb_elastic_marker_update(&marker, 1, 60, 10, 180, 0);
    assert(marker.left == 30 && marker.right == 30); // No retarget jump.
    brb_elastic_marker_update(&marker, 1, 60, 10, 180, 20);
    assert(marker.right > marker.left && marker.left > 30); // Head leads tail.
    float left = marker.left, right = marker.right;
    brb_elastic_marker_update(&marker, 1, 15, 10, 180, 20);
    assert(marker.left == left && marker.right == right); // Reversal preserves shape.
    for (uint32_t now = 40; now < 1000; now += 20)
        brb_elastic_marker_update(&marker, 1, 15, 10, 180, now);
    assert(marker.left == 15 && marker.right == 15); // Returns to a perfect circle.
    // Exact spring evolution is independent of frame subdivisions.
    BrbElasticMarker split = {0}, batch = {0};
    brb_elastic_marker_update(&batch, 1, 30, 10, 180, 0);
    brb_elastic_marker_update(&batch, 1, 90, 10, 180, 0);
    split = batch;
    brb_elastic_marker_update(&batch, 1, 90, 10, 180, 60);
    for (uint32_t now = 10; now <= 60; now += 10)
        brb_elastic_marker_update(&split, 1, 90, 10, 180, now);
    assert(fabsf(batch.left - split.left) < 0.001f);
    assert(fabsf(batch.right - split.right) < 0.001f);
    // Rapid crank sweeps, wrapping, reversals and timestamp wrap stay bounded.
    for (uint32_t frame = 0; frame < 20000; ++frame) {
        const float target = 10 + (float)((frame * 73) % 171);
        brb_elastic_marker_update(&marker, 1, target, 10, 180,
            UINT32_MAX - 200 + frame * 7);
        check(&marker);
    }
    brb_elastic_marker_update(&marker, 2, 100, 10, 180, marker.at);
    assert(marker.left == 100 && marker.right == 100); // No cross-group bridge.
    brb_elastic_marker_update(&marker, 2, 140, 10, 180, marker.at + 1000);
    assert(marker.left == 140 && marker.right == 140); // Pause/resume cannot fling.
    marker.active = false;
    brb_elastic_marker_update(&marker, 2, 20, 10, 180, marker.at);
    assert(marker.left == 20 && marker.right == 20);
    assert(brb_elastic_marker_settled(&marker,20,20));
    assert(!brb_elastic_marker_settled(&marker,20,22));
    marker.right=22;
    assert(brb_elastic_marker_settled(&marker,20,22)); // Loaded pressure can rest off-center.
    marker.right_velocity=1;
    assert(!brb_elastic_marker_settled(&marker,20,22));
    marker.right_velocity=0;marker.active=false;
    assert(!brb_elastic_marker_settled(&marker,20,22));
    test_jelly();
    puts("elastic marker: legacy regression, speed-driven jelly, bounded entry/bounce, finite settle and 40000 rapid sweeps passed");
}
