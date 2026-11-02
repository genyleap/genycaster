// Streaks are based only on server-confirmed Cast events. Never grant a day
// for logging in or for client-supplied timestamps, IDs or activity counts.
// Versioned, server-authored badge catalogue; no claimable awards or tokens.
export const STREAK_MILESTONES = Object.freeze([1,3,7,14,30,60,90,100,180,365]);
export const STREAK_BADGE_CATALOG = Object.freeze(
  STREAK_MILESTONES.map(days=>Object.freeze({
    id:'streak-'+days,threshold_days:days,category:'streak',
    icon:'flame',reward_kind:'none'
  }))
);
const DAY_MS=86_400_000;
export function calculateStreaks(rows,now=new Date()) {
  const today=Math.floor(now.getTime()/DAY_MS);
  const unique={activity:new Set(),creator:new Set()};
  for(const row of rows||[]) {
    if(!row || !/^\d{4}-\d{2}-\d{2}$/.test(String(row.day)))continue;
    const day=Math.floor(Date.parse(row.day+'T00:00:00Z')/DAY_MS);
    if(!Number.isInteger(day)||day>today||day<today-730)continue;
    if(row.kind!=='cast' && row.kind!=='reply')continue;
    unique.activity.add(day);
    if(row.kind==='cast')unique.creator.add(day);
  }
  const summarize=(values)=>{
    const all=[...values].sort((a,b)=>a-b);
    let longest=0, sequence=0, previous=-999999;
    for(const day of all) {
      sequence=day===previous+1?sequence+1:1;
      longest=Math.max(longest,sequence);
      previous=day;
    }
    // Missing today is not a broken streak until the UTC day ends.
    let end=values.has(today)?today:today-1;
    let current=0;
    while(values.has(end) && current<=730){++current;--end;}
    const badges=STREAK_MILESTONES.filter(m=>longest>=m);
    // UI consumes this list rather than hardcoding levels. All progress
    // comes from verified historical Casts, never a client-supplied score.
    const milestone_definitions=STREAK_BADGE_CATALOG.map(badge=>({
      ...badge,unlocked:longest>=badge.threshold_days,
      progress_days:Math.min(longest,badge.threshold_days)
    }));
    const createWeek=(firstDay)=>{
      const weekday=new Date(today*DAY_MS).getUTCDay();
      const start=today-((weekday-firstDay+7)%7);
      return Array.from({length:7},(_,i)=>{
        const day=start+i;
        return {date:new Date(day*DAY_MS).toISOString().slice(0,10),
          completed:values.has(day),future:day>today};
      });
    };
    const week=createWeek(1);          // Monday–Sunday, existing clients
    const week_saturday=createWeek(6); // Saturday–Friday, Persian/Arabic RTL
    const next=STREAK_MILESTONES.find(n=>n>longest)||null;
    return {current,longest,total_days:all.length,badges,next_milestone:next,milestone_definitions,week,week_saturday};
  };
  return {time_zone:'UTC',activity:summarize(unique.activity),
    creator:summarize(unique.creator)};
}
