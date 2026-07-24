#pragma once

namespace ShelfManager::Presentation {

// 左側Navigation Tabで切り替えるMVP画面の識別子。
// 画面選択だけを表し、認証や操作許可を兼ねない。
enum class ScreenId {
    VisualRack,
    MachiningQueue,
    ManualTransport
};

}  // namespace ShelfManager::Presentation
