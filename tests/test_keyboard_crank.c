#include "keyboard_crank.h"
#include <assert.h>
#include <stdio.h>

static int feed(HelloKeyboardCrankDetent* state,int index,int length,float angle) {
    assert(hello_keyboard_crank_add(state,angle));
    for(unsigned n=0;n<32;n++) {
        const int direction=state->remainder<0?-1:1;
        const bool wraps=direction>0?index==length-1:index==0;
        const int step=hello_keyboard_crank_take(state,wraps);
        if(!step)return index;
        index=(index+step+length)%length;
    }
    assert(false);return index;
}

static void test_wrap_reversal_partitions(void) {
    const int lengths[]={2,9,10,21,29,40,64};
    // Reverse repeatedly before, through, and after the loaded release well.
    // No sign change may discard pressure or reset the release slope, and
    // splitting the same physical movement cannot change the result.
    const float changes[]={23,-4,3,-16,0.005f,-0.005f,-72,9,34,-38,17,-65,
        110,-135,63,-18,240,-250,51,-8};
    for(unsigned g=0;g<sizeof(lengths)/sizeof(lengths[0]);g++)
    for(int direction=-1;direction<=1;direction+=2)
    for(int advance=0;advance<6;advance++) {
        const int length=lengths[g];
        HelloKeyboardCrankDetent batch={0},slices={0},mirror={0};
        int a=direction>0?length-1:0,b=a,c=length-1-a;
        const float edge=hello_keyboard_crank_threshold(&batch,true)+0.01f;
        a=feed(&batch,a,length,(float)direction*edge);
        b=feed(&slices,b,length,(float)direction*edge);
        c=feed(&mirror,c,length,-(float)direction*edge);
        for(int n=0;n<advance;n++) {
            const bool wraps=direction>0?a==length-1:a==0;
            const float angle=hello_keyboard_crank_threshold(&batch,wraps)+0.01f;
            a=feed(&batch,a,length,(float)direction*angle);
            b=feed(&slices,b,length,(float)direction*angle);
            c=feed(&mirror,c,length,-(float)direction*angle);
        }
        for(unsigned n=0;n<sizeof(changes)/sizeof(changes[0]);n++) {
            const float angle=(float)direction*changes[n];
            a=feed(&batch,a,length,angle);
            c=feed(&mirror,c,length,-angle);
            for(int part=0;part<100;part++)b=feed(&slices,b,length,angle/100);
            assert(a==b&&a+c==length-1);
            assert(batch.release_steps==slices.release_steps&&batch.release_steps==mirror.release_steps);
            assert(batch.wrapped==slices.wrapped&&batch.wrapped==mirror.wrapped);
            assert(batch.direction==slices.direction&&batch.direction==-mirror.direction);
            assert(fabsf(batch.remainder-slices.remainder)<0.008f);
            assert(fabsf(batch.remainder+mirror.remainder)<0.008f);
        }
        for(int n=0;n<1000;n++) {
            assert(feed(&batch,a,length,0)==a);
            assert(feed(&mirror,c,length,0)==c);
        }
    }
}

static void test_five_item_release_and_reversal(void) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    const float factors[][5]={{2.2888889f,2.2386406f,1.9294705f,1.4104015f,1.20f},
        {1.90f,1.8676975f,1.6689453f,1.3352581f,1.20f}};
    for(int direction=-1;direction<=1;direction+=2)
    for(unsigned p=0;p<sizeof(factors)/sizeof(factors[0]);p++) {
        HelloKeyboardCrankDetent state={.direction=-direction};
        if(p==0) {
            state.remainder=(float)direction*hello_keyboard_crank_threshold(&state,true);
            assert(hello_keyboard_crank_take(&state,true)==direction);
            assert(state.release_steps==5);
        }
        for(int item=0;item<5;item++) {
            const float expected=tick*factors[p][item];
            state.remainder=(float)direction*0.001f;
            const float threshold=hello_keyboard_crank_threshold(&state,false);
            assert(fabsf(threshold-expected)<0.0001f);
            const int steps=state.release_steps;
            const float peak=state.release_peak;
            // Noise and idle may unload pressure but never restart a taper.
            assert(hello_keyboard_crank_add(&state,-(float)direction*0.0005f));
            assert(!hello_keyboard_crank_take(&state,false));
            assert(state.release_steps==steps&&state.release_peak==peak);
            state.remainder=(float)direction*(threshold+0.001f);
            assert(hello_keyboard_crank_take(&state,false)==direction);
            assert(state.release_steps==4-item);
        }
        assert(fabsf(hello_keyboard_crank_threshold(&state,false)-tick*1.20f)<0.0001f);
    }
    // Reverse a loaded wrap release: carry its higher current strength into a
    // new five-item curve without adding another barrier or changing pressure.
    HelloKeyboardCrankDetent state={.direction=1,.release_steps=4,
        .release_peak=HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR,.remainder=3};
    const float current=hello_keyboard_crank_release_factor(&state);
    assert(hello_keyboard_crank_add(&state,-3.5f));
    assert(state.remainder==-0.5f&&state.release_steps==4);
    assert(fabsf(hello_keyboard_crank_threshold(&state,false)-tick*current)<0.0001f);
    assert(hello_keyboard_crank_add(&state,-tick*current));
    assert(hello_keyboard_crank_take(&state,false)==-1);
    assert(state.release_steps==4&&state.release_peak==current);
    assert(fabsf(state.remainder+0.5f)<0.0001f);
}

static float threshold_in_direction(const HelloKeyboardCrankDetent* state,int direction,bool wraps) {
    HelloKeyboardCrankDetent prospective=*state;prospective.remainder=(float)direction;
    return hello_keyboard_crank_threshold(&prospective,wraps);
}
static void test_rapid_intragroup_reversal_inertia(void) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    const uint32_t frame_ms[]={16,20,40};
    for(unsigned cadence=0;cadence<sizeof(frame_ms)/sizeof(frame_ms[0]);cadence++)
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboardCrankDetent batch={.direction=direction},slices=batch,mirror={.direction=-direction};
        int a=32,b=32,c=31;uint32_t now=1000;
        hello_keyboard_crank_sample_velocity(&batch,0,now);
        hello_keyboard_crank_sample_velocity(&slices,0,now);
        hello_keyboard_crank_sample_velocity(&mirror,0,now);
        // Between the old 1.55 well (even at its 10% cap) and the requested
        // 1.90 reversal well: quick balanced turns must not select or ratchet.
        const float small=tick*1.75f;
        assert(small>tick*1.55f*1.10f&&small<tick*1.90f);
        for(unsigned pulse=0;pulse<200;pulse++) {
            const float angle=(float)direction*(pulse%2?small:-small);now+=frame_ms[cadence];
            hello_keyboard_crank_sample_velocity(&batch,angle,now);
            hello_keyboard_crank_sample_velocity(&slices,angle,now);
            hello_keyboard_crank_sample_velocity(&mirror,-angle,now);
            a=feed(&batch,a,64,angle);c=feed(&mirror,c,64,-angle);
            for(unsigned part=0;part<100;part++)b=feed(&slices,b,64,angle/100);
            assert(a==32&&b==32&&c==31);
            assert(batch.direction==direction&&mirror.direction==-direction);
            assert(!batch.release_steps&&!slices.release_steps&&!mirror.release_steps&&!batch.wrapped&&!mirror.wrapped);
            assert(batch.speed_factor>=1&&batch.speed_factor<=1.10f);
            assert(batch.speed_factor==slices.speed_factor&&batch.speed_factor==mirror.speed_factor);
            assert(fabsf(batch.remainder-slices.remainder)<0.008f&&fabsf(batch.remainder+mirror.remainder)<0.0001f);
        }
        assert(fabsf(batch.speed_factor-1.10f)<0.00001f);
        // A genuine reversal consumes one well, then each rapid consumed
        // change rearms the same five-item inertia without touching a boundary.
        for(unsigned turn=0;turn<100;turn++) {
            const int step=-batch.direction,previous=a;
            const float threshold=threshold_in_direction(&batch,step,false);
            assert(fabsf(threshold-tick*1.90f*batch.speed_factor)<0.0001f);
            const float angle=(float)step*(threshold+0.001f)-batch.remainder;
            now+=frame_ms[cadence];
            hello_keyboard_crank_sample_velocity(&batch,angle,now);
            hello_keyboard_crank_sample_velocity(&slices,angle,now);
            hello_keyboard_crank_sample_velocity(&mirror,-angle,now);
            a=feed(&batch,a,64,angle);c=feed(&mirror,c,64,-angle);
            for(unsigned part=0;part<100;part++)b=feed(&slices,b,64,angle/100);
            assert(a==previous+step&&a==b&&a+c==63);
            assert(batch.release_steps==4&&slices.release_steps==4&&mirror.release_steps==4);
            assert(fabsf(batch.release_peak-1.90f)<0.00001f&&batch.release_peak==mirror.release_peak);
            assert(batch.direction==slices.direction&&batch.direction==-mirror.direction);
            assert(!batch.wrapped&&!slices.wrapped&&!mirror.wrapped);
            assert(fabsf(batch.remainder-slices.remainder)<0.008f&&fabsf(batch.remainder+mirror.remainder)<0.0001f);
            assert(batch.speed_factor<=1.10f&&batch.speed_factor==mirror.speed_factor);
        }
        for(unsigned idle=0;idle<1000;idle++)assert(feed(&batch,a,64,0)==a);
    }
    // A stronger unfinished wrap taper wins over reversal resistance; a lower
    // taper gets precisely the new reversal peak, never two stacked barriers.
    for(int steps=0;steps<=5;steps++)for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboardCrankDetent state={.direction=-direction,.release_steps=steps,
            .release_peak=HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR,.wrapped=true};
        const float peak=fmaxf(hello_keyboard_crank_release_factor(&state),1.90f);
        const float threshold=threshold_in_direction(&state,direction,false);
        assert(fabsf(threshold-tick*peak)<0.0001f);
        state.remainder=(float)direction*(threshold-0.001f);
        assert(!hello_keyboard_crank_take(&state,false)&&state.release_steps==steps);
        assert(hello_keyboard_crank_add(&state,(float)direction*0.002f));
        assert(hello_keyboard_crank_take(&state,false)==direction);
        assert(state.release_steps==4&&fabsf(state.release_peak-peak)<0.00001f&&state.wrapped);
    }
}
static void test_identical_wrap_resistance_across_groups(void) {
    const int lengths[]={9,10,21,40,64};const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    for(int seen=0;seen<2;seen++)for(int sign=-1;sign<=1;sign+=2)
    for(int previous=-1;previous<=1;previous++)for(int phase=0;phase<=5;phase++)for(int fast=0;fast<2;fast++) {
        const float speed=fast?1.10f:1.0f;
        const float expected=tick*3.30f*(seen?1:0.80f)*speed;
        float reference=0;
        for(unsigned group=0;group<sizeof(lengths)/sizeof(lengths[0]);group++) {
            HelloKeyboardCrankDetent state={.direction=sign*previous,.release_steps=phase,
                .release_peak=HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR,.speed_factor=speed,.wrapped=seen!=0};
            const int length=lengths[group],edge=sign>0?length-1:0;
            const float threshold=threshold_in_direction(&state,sign,true);
            if(!group)reference=threshold;
            assert(threshold==reference&&fabsf(threshold-expected)<0.0001f);
            int index=feed(&state,edge,length,(float)sign*(threshold-0.001f));
            assert(index==edge&&state.wrapped==(seen!=0));
            index=feed(&state,index,length,(float)sign*0.002f);
            assert(index==(sign>0?0:length-1)&&state.wrapped&&state.release_steps==5);
            assert(state.release_peak==HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR);
            assert(fabsf(threshold_in_direction(&state,-sign,true)-tick*3.30f*speed)<0.0001f);
            for(unsigned idle=0;idle<4;idle++)assert(feed(&state,index,length,0)==index);
        }
    }
}

static void test_first_wrap_only_is_softer(void) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    const float full=tick*HELLO_KEYBOARD_CRANK_RUNNING_FACTOR*HELLO_KEYBOARD_CRANK_EDGE_FACTOR;
    const float first=full*HELLO_KEYBOARD_CRANK_FIRST_EDGE_FACTOR;
    assert(first>76.72f&&first<76.74f&&first>tick*HELLO_KEYBOARD_CRANK_RUNNING_FACTOR);
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboardCrankDetent state={.direction=direction};
        state.remainder=(float)direction;
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-first)<0.0001f);
        assert(fabsf(hello_keyboard_crank_threshold(&state,false)-tick*1.20f)<0.0001f);
        state.remainder=(float)direction*(first-0.001f);
        assert(!hello_keyboard_crank_take(&state,true)&&!state.wrapped);
        assert(hello_keyboard_crank_add(&state,(float)direction*0.002f));
        assert(hello_keyboard_crank_take(&state,true)==direction&&state.wrapped);
        assert(state.release_steps==HELLO_KEYBOARD_CRANK_RELEASE_ITEMS);
        assert(state.release_peak==HELLO_KEYBOARD_CRANK_WRAP_RELEASE_FACTOR);
        state.remainder=(float)direction;
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-full)<0.0001f);
        // Idle, velocity reset (including a visual gate), curve progress and
        // physical reversals cannot make a later edge count as first again.
        assert(hello_keyboard_crank_add(&state,0)&&state.wrapped);
        hello_keyboard_crank_velocity_reset(&state);assert(state.wrapped);
        state.remainder=-(float)direction*1;
        const float reverse=hello_keyboard_crank_threshold(&state,false);
        state.remainder=-(float)direction*(reverse+0.001f);
        assert(hello_keyboard_crank_take(&state,false)==-direction&&state.wrapped);
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-full)<0.0001f);
        state.release_steps=0;state.remainder=(float)direction;
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-full)<0.0001f);
        state.remainder=(float)direction*(full-0.001f);
        assert(!hello_keyboard_crank_take(&state,true)&&state.wrapped);
        assert(hello_keyboard_crank_add(&state,(float)direction*0.002f));
        assert(hello_keyboard_crank_take(&state,true)==direction&&state.wrapped);
        hello_keyboard_crank_reset(&state);assert(!state.wrapped);
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-first)<0.0001f);
        state.speed_factor=HELLO_KEYBOARD_CRANK_SPEED_MAX_FACTOR;
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-first*1.10f)<0.0001f);
        state.wrapped=true;
        assert(fabsf(hello_keyboard_crank_threshold(&state,true)-full*1.10f)<0.0001f);
    }
}

static void test_physical_velocity(void) {
    const float speeds[]={180,360,540,720,1080,2160};
    for(unsigned i=0;i<sizeof(speeds)/sizeof(speeds[0]);i++) {
        HelloKeyboardCrankDetent a={0},b={0},mirror={0};
        hello_keyboard_crank_sample_velocity(&mirror,0,0);
        hello_keyboard_crank_sample_velocity(&a,0,0);
        hello_keyboard_crank_sample_velocity(&b,0,0);
        for(uint32_t now=40;now<=2000;now+=40) {
            hello_keyboard_crank_sample_velocity(&a,speeds[i]*0.04f,now);
            hello_keyboard_crank_sample_velocity(&mirror,-speeds[i]*0.04f,now);
            assert(a.velocity==mirror.velocity&&a.speed_factor==mirror.speed_factor);
            hello_keyboard_crank_sample_velocity(&b,speeds[i]*0.02f,now-20);
            hello_keyboard_crank_sample_velocity(&b,speeds[i]*0.02f,now);
            assert(fabsf(a.velocity-b.velocity)<0.001f);
            assert(fabsf(a.speed_factor-b.speed_factor)<0.00001f);
        }
        if(speeds[i]<=360)assert(a.speed_factor==1);
        if(speeds[i]==720)assert(fabsf(a.speed_factor-1.05f)<0.00001f);
        if(speeds[i]>=1080)assert(fabsf(a.speed_factor-1.10f)<0.00001f);
        assert(a.speed_factor>=1&&a.speed_factor<=1.10f);
        const float factor=a.speed_factor;
        a.direction=1;a.remainder=1;
        const float ordinary=hello_keyboard_crank_threshold(&a,false);
        assert(fabsf(ordinary-HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM*1.2f*factor)<0.0001f);
        assert(fabsf(hello_keyboard_crank_threshold(&a,true)-ordinary*2.75f*0.80f)<0.0001f);
        a.wrapped=true;
        assert(fabsf(hello_keyboard_crank_threshold(&a,true)-ordinary*2.75f)<0.0001f);
        // No idle decay may consume stored angular pressure. The next real
        // sample sees the decayed velocity and starts with ordinary resistance.
        a.remainder=ordinary*0.999f;
        for(uint32_t now=2020;now<4000;now+=20) {
            hello_keyboard_crank_sample_velocity(&a,0,now);
            assert(a.speed_factor==factor);
            assert(!hello_keyboard_crank_take(&a,false));
        }
        hello_keyboard_crank_sample_velocity(&a,0.001f,4000);
        assert(a.speed_factor==1);
        hello_keyboard_crank_velocity_reset(&a);
        assert(a.velocity==0&&a.speed_factor==1&&!a.sampled);
        hello_keyboard_crank_sample_velocity(&a,360,4020);
        assert(a.velocity==0&&a.speed_factor==1); // Discarded gate speed cannot leak.
    }
    HelloKeyboardCrankDetent state={0};
    hello_keyboard_crank_sample_velocity(&state,0,UINT32_MAX-9);
    hello_keyboard_crank_sample_velocity(&state,36,10); // Clock rollover:20ms.
    assert(state.velocity>398&&state.velocity<399);
    const float velocity=state.velocity,factor=state.speed_factor;
    hello_keyboard_crank_sample_velocity(&state,360,10);
    assert(state.velocity==velocity&&state.speed_factor==factor);
    hello_keyboard_crank_sample_velocity(&state,360,1000); // Long pause is not speed.
    assert(state.velocity==0&&state.speed_factor==1);
}

static void test_immediate_entry_retains_angular_breakaway(void) {
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    for(int sign=-1;sign<=1;sign+=2)for(int reverse=0;reverse<2;reverse++) {
        HelloKeyboardCrankDetent state={.direction=sign,.entry_breakaway=true};
        const int direction=reverse?-sign:sign;
        state.remainder=(float)direction;
        const float threshold=hello_keyboard_crank_threshold(&state,false);
        assert(fabsf(threshold-tick*(reverse?1.90f:1.55f))<0.0001f);
        state.remainder=(float)direction*(threshold-0.001f);
        assert(!hello_keyboard_crank_take(&state,false)&&state.entry_breakaway&&state.direction==sign);
        assert(hello_keyboard_crank_add(&state,(float)direction*0.002f));
        assert(hello_keyboard_crank_take(&state,false)==direction&&!state.entry_breakaway);
        assert(state.direction==direction&&state.release_steps==(reverse?4:0));
        assert(!hello_keyboard_crank_take(&state,false));
        // Entry softness never compounds the true-edge wall or its speed cap.
        for(int wrapped=0;wrapped<2;wrapped++) {
            HelloKeyboardCrankDetent a={.direction=sign,.entry_breakaway=true,.wrapped=wrapped,.speed_factor=1.10f,.remainder=(float)direction};
            HelloKeyboardCrankDetent b=a;b.entry_breakaway=false;
            assert(hello_keyboard_crank_threshold(&a,true)==hello_keyboard_crank_threshold(&b,true));
        }
    }
}

int main(void) {
    test_immediate_entry_retains_angular_breakaway();
    test_wrap_reversal_partitions();
    test_five_item_release_and_reversal();
    test_rapid_intragroup_reversal_inertia();
    test_identical_wrap_resistance_across_groups();
    test_physical_velocity();
    test_first_wrap_only_is_softer();
    const float tick=HELLO_KEYBOARD_CRANK_DEGREES_PER_ITEM;
    assert(tick>29.06f&&tick<29.07f);
    HelloKeyboardCrankDetent state={0};
    const float start=tick*1.55f, reversal=tick*1.90f, running=tick*1.20f, edge=tick*3.30f;
    assert(start>45.04f&&start<45.05f&&running>34.87f&&running<34.88f);
    assert(edge>95.90f&&edge<95.92f);
    state.wrapped=true;
    for(int direction=-1;direction<=1;direction++)
    for(int release=0;release<=HELLO_KEYBOARD_CRANK_RELEASE_ITEMS;release++) {
        state.direction=direction;state.release_steps=release;
        for(int sign=-1;sign<=1;sign+=2) {
            state.remainder=(float)sign;
            assert(fabsf(hello_keyboard_crank_threshold(&state,true)-edge)<0.0001f);
        }
    }
    state.direction=1;
    assert(fabsf(hello_keyboard_crank_threshold(&state,true)/running-2.75f)<0.0001f);
    hello_keyboard_crank_reset(&state);
    assert(hello_keyboard_crank_add(&state,start*0.25f));
    assert(fabsf(hello_keyboard_crank_preload(&state,false)-0.15625f)<0.0001f);
    assert(!hello_keyboard_crank_take(&state,false));
    assert(hello_keyboard_crank_add(&state,start*0.5f));
    assert(fabsf(hello_keyboard_crank_preload(&state,false)-0.84375f)<0.0001f);
    for(int i=0;i<1000;i++){assert(hello_keyboard_crank_add(&state,0));assert(!hello_keyboard_crank_take(&state,false));}
    assert(hello_keyboard_crank_add(&state,start*0.25f+0.001f));
    assert(hello_keyboard_crank_take(&state,false)==1);
    assert(fabsf(hello_keyboard_crank_preload(&state,false))<0.0001f);
    // Build 63 consumed two 29-degree detents in this ordinary first turn.
    // The mechanical breakaway now exposes character two before character three.
    hello_keyboard_crank_reset(&state);
    int first=feed(&state,0,10,65);
    assert(first==1);
    for(int i=0;i<1000;i++)assert(feed(&state,first,10,0)==first);
    assert(feed(&state,first,10,10)==first);
    first=feed(&state,first,10,5);assert(first==2);
    hello_keyboard_crank_reset(&state);
    assert(feed(&state,4,10,-65)==3); // The first turn is symmetric.
    hello_keyboard_crank_reset(&state);
    for(int i=0;i<10000;i++) {
        assert(feed(&state,4,10,0.25f)==4);
        assert(feed(&state,4,10,-0.25f)==4);
    }
    assert(state.remainder==0&&state.direction==0);
    // Every detent retains a full physical well, and reversing requires real
    // travel through the loaded well rather than resetting its origin.
    hello_keyboard_crank_reset(&state);state.direction=1;
    assert(feed(&state,4,10,running*0.7f)==4);
    const float loaded=state.remainder;
    for(int i=0;i<10000;i++) {
        assert(feed(&state,4,10,-0.25f)==4);
        assert(feed(&state,4,10,0.25f)==4);
    }
    assert(fabsf(state.remainder-loaded)<0.0001f);
    assert(feed(&state,4,10,-loaded)==4&&fabsf(state.remainder)<0.0001f);
    assert(feed(&state,4,10,-reversal*0.99f)==4);
    assert(feed(&state,4,10,-reversal*0.02f)==3);
    const int lengths[]={29,9,9,9,10,21};
    const float angles[]={32,81,-4,-21,-77,15,103,-65,24,61,-9,4,-125,146};
    for(unsigned g=0;g<sizeof(lengths)/sizeof(lengths[0]);g++) {
        HelloKeyboardCrankDetent batch={0},slices={0};int a=lengths[g]-1,b=a;
        for(unsigned i=0;i<sizeof(angles)/sizeof(angles[0]);i++) {
            a=feed(&batch,a,lengths[g],angles[i]);
            for(int n=0;n<100;n++)b=feed(&slices,b,lengths[g],angles[i]/100);
            assert(a==b&&batch.release_steps==slices.release_steps&&batch.direction==slices.direction);
            assert(fabsf(batch.remainder-slices.remainder)<0.004f);
        }
        hello_keyboard_crank_reset(&state);int index=lengths[g]-1;
        index=feed(&state,index,lengths[g],edge+0.1f);
        assert(index==0&&state.release_steps==HELLO_KEYBOARD_CRANK_RELEASE_ITEMS);
        index=feed(&state,index,lengths[g],tick*0.5f);assert(index==0);
        const float before=hello_keyboard_crank_threshold(&state,false),pressure=state.remainder;
        assert(hello_keyboard_crank_add(&state,-1));
        assert(state.remainder==pressure-1&&state.release_steps==HELLO_KEYBOARD_CRANK_RELEASE_ITEMS);
        assert(hello_keyboard_crank_threshold(&state,false)==before);
    }
    // Frame rates map the same physical 360 deg/s gesture to the same detents.
    // No clock, dwell timer, catch-up queue or sampling-rate gain is involved.
    for(int direction=-1;direction<=1;direction+=2) {
        HelloKeyboardCrankDetent slow={0},fast={0};int a=4,b=4;
        for(int frame=0;frame<50;frame++) {
            a=feed(&slow,a,10,(float)direction*14.4f);
            b=feed(&fast,b,10,(float)direction*7.2f);
            b=feed(&fast,b,10,(float)direction*7.2f);
            assert(a==b&&slow.direction==fast.direction&&slow.release_steps==fast.release_steps);
            assert(fabsf(slow.remainder-fast.remainder)<0.002f);
        }
        for(int i=0;i<1000;i++)assert(feed(&slow,a,10,0)==a);
    }
    // Pathological finite SDK changes are bounded and completely drained now.
    hello_keyboard_crank_reset(&state);
    int bounded=feed(&state,0,10,1e30f);
    for(int i=0;i<1000;i++)assert(feed(&state,bounded,10,0)==bounded);
    BrbElasticMarker marker={0};
    hello_keyboard_crank_marker_update(&marker,1,20,0,5,150,100);
    hello_keyboard_crank_marker_update(&marker,1,20,8,5,150,116);
    assert(marker.right>20&&marker.left==20);
    const float before=marker.right,velocity=marker.right_velocity;
    hello_keyboard_crank_marker_update(&marker,1,20,-8,5,150,116);
    assert(marker.right==before&&marker.right_velocity==velocity); // Same-time reversal cannot jump.
    // Crossing zero pressure cannot change the spring law. Even while the
    // previous character transition is still settling, an arbitrarily small
    // reversal must produce an arbitrarily small change on either cap.
    BrbElasticMarker positive={.left=40,.right=60,.left_velocity=1,.right_velocity=-1,
        .target=50,.at=100,.tag=1,.active=true};
    BrbElasticMarker negative=positive;
    hello_keyboard_crank_marker_update(&positive,1,50,0.0001f,5,150,116);
    hello_keyboard_crank_marker_update(&negative,1,50,-0.0001f,5,150,116);
    assert(fabsf(positive.left-negative.left)<0.0001f);
    assert(fabsf(positive.right-negative.right)<0.0001f);
    assert(fabsf(positive.left_velocity-negative.left_velocity)<0.001f);
    assert(fabsf(positive.right_velocity-negative.right_velocity)<0.002f);
    for(uint32_t now=132;now<10000;now+=16) {
        const float pull=(now/64)%2?8:-8;
        hello_keyboard_crank_marker_update(&marker,1,(float)(20+(now/300)%5*20),pull,5,150,now);
        assert(marker.left>=5&&marker.right<=150&&isfinite(marker.left)&&isfinite(marker.right));
    }
    assert(!hello_keyboard_crank_add(&state,NAN)&&!state.remainder&&!state.release_steps);
    puts("keyboard crank mechanics: five-item wrap/reversal curves, physical velocity cap, reversible pressure, frame invariance and no queued input passed");
}
