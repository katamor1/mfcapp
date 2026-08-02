#pragma once

#include <atomic>
#include <memory>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 最新のイミュータブルなMachineSnapshotをApplication内で共有するPublication境界。
// PublishとCurrentは複数スレッドから同時に呼び出せるが、Snapshotの内容検証、
// 変更通知、待機、履歴保持は行わない。
//
// THREAD: 公開済みSnapshot自体は変更せず、shared_ptrをアトミックに差し替える。
// 所有権: Storeは最新Snapshotを共有所有し、Currentの戻り値を保持する呼出し側は
// そのSnapshotの寿命を独立して延長できる。
class MachineSnapshotStore final {
public:
    MachineSnapshotStore() = default;

    // snapshotを最新値としてアトミックに公開する。
    // null、または現在値以下のSnapshotVersionは受け付けずConflictを返す。
    // 成功はその時点でStore内の差替えが完了したことだけを示し、直後に別Publisherの
    // より新しいSnapshotへ置き換わる可能性がある。画面通知や外部書込みは行わない。
    // RackState、Workpiece、搬送先などの相互整合はProducerが公開前に保証すること。
    [[nodiscard]] ShelfManager::Domain::Result<void> Publish(
        std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot);

    // 呼出し時点で公開されている最新Snapshotの共有所有権を取得する。
    // 初回Publish前はnullを返す。通知に含まれるVersionより新しいSnapshotを返すことが
    // あり得るため、通知Versionの状態を再生する用途には使用しない。
    // 取得後のSnapshotは後続Publishで変更されず、Store内部への参照も公開しない。
    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    // WHY: C++17ではshared_ptr専用のatomic wrapperではなく、
    // shared_ptr向けのatomic free functionを使用して同時アクセスを保護する。
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> latest_;
};

}  // namespace ShelfManager::Application
