#pragma once

namespace ShelfManager::Presentation {

// 左側Navigation Tabで切り替えるMVP画面の識別子。
// 画面選択だけを表し、認証、入力Focus、Presenterの生存、外部操作許可を兼ねない。
// enumの順序・整数値をCommand ID、Resource ID、永続設定、監査値として使用せず、
// MFC境界でTab／Windowとの対応を明示的に定義する。
enum class ScreenId {
    VisualRack,
    MachiningQueue,
    ManualTransport
};

}  // namespace ShelfManager::Presentation
