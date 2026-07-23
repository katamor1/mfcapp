#pragma once

#include <atomic>
#include <memory>

#include "ShelfManager/Domain/MachineSnapshot.h"
#include "ShelfManager/Domain/Result.h"

namespace ShelfManager::Application {

// 最新のイミュータブルなMachineSnapshotをApplication内で共有する。
// PublishとCurrentは複数スレッドから同時に呼び出せる。
//
// THREAD: 公開済みSnapshot自体は変更せず、shared_ptrをアトミックに差し替える。
// 所有権: Storeは最新Snapshotを共有所有し、Currentの戻り値を保持する呼出し側は
// そのSnapshotの寿命を独立して延長できる。
class MachineSnapshotStore final {
public:
    MachineSnapshotStore() = default;

    // snapshotを最新値としてアトミックに公開する。
    // null、または現在値以下のSnapshotVersionは受け付けずConflictを返す。
    // 成功はStore内の差替え完了だけを示し、画面通知や外部書込みは行わない。
    [[nodiscard]] ShelfManager::Domain::Result<void> Publish(
        std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> snapshot);

    // 現在公開されているSnapshotを取得する。
    // 初回Publish前はnullを返し、取得後のSnapshotは後続Publishで変更されない。
    [[nodiscard]] std::shared_ptr<const ShelfManager::Domain::MachineSnapshot>
    Current() const noexcept;

private:
    // WHY: C++17ではshared_ptr専用のatomic wrapperではなく、
    // shared_ptr向けのatomic free functionを使用して同時アクセスを保護する。
    std::shared_ptr<const ShelfManager::Domain::MachineSnapshot> latest_;
};

}  // namespace ShelfManager::Application
