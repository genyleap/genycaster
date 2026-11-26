#include <cassert>
#include <chrono>

import farcaster.application.authentication;
import farcaster.application.session;
import farcaster.foundation.error;

int main()
{
    using namespace farcaster::application::authentication;
    using namespace std::chrono_literals;
    namespace session=farcaster::application::session;
    namespace error=farcaster::foundation::error;
    AuthenticationCoordinator coordinator;
    const AuthenticationCoordinator::Time now{};
    Snapshot snapshot{.principal="protocol-a:42",.signerReady=true,.cloudReady=true};
    const auto initial=coordinator.plan(Trigger::Foreground,snapshot,now);
    assert(initial.revalidateSigner&&initial.restoreCloud);
    assert(!coordinator.plan(Trigger::Timer,snapshot,now+30s).revalidateSigner);
    assert(coordinator.plan(Trigger::Timer,snapshot,now+300s).revalidateSigner);
    snapshot.offline=true;
    assert(!coordinator.plan(Trigger::ConnectivityRestored,snapshot,now+600s).revalidateSigner);
    snapshot.offline=false;
    snapshot.signerReady=false;snapshot.signerRetryable=true;snapshot.cloudReady=false;
    auto actions=coordinator.plan(Trigger::ConnectivityRestored,snapshot,now+600s);
    assert(actions.revalidateSigner&&actions.restoreCloud);
    actions=coordinator.plan(Trigger::Timer,snapshot,now+601s);
    assert(!actions.revalidateSigner&&!actions.restoreCloud);
    assert(coordinator.plan(Trigger::Timer,snapshot,now+630s).restoreCloud);
    snapshot.signerBusy=true;
    assert(!coordinator.plan(Trigger::ManualSigner,snapshot,now+631s).revalidateSigner);
    snapshot.principal="protocol-b:42";snapshot.signerBusy=false;
    assert(coordinator.plan(Trigger::Foreground,snapshot,now+632s).revalidateSigner);
    const auto ticket=coordinator.beginRestore(now);
    assert(ticket&&!coordinator.beginRestore(now));
    assert(coordinator.completeRestore(*ticket,session::SessionState{.phase=session::SessionPhase::Failed,
        .error=error::Error::make(error::ErrorCode::Unavailable,"vault locked")},now));
    assert(!coordinator.beginRestore(now+1s));
    auto retry=coordinator.beginRestore(now+30s);
    assert(retry);
    coordinator.cancelRestore();
    assert(!coordinator.completeRestore(*retry,session::SessionState{},now+30s));
    assert(!coordinator.beginRestore(now+600s));
}
