#pragma once

#include <Windows.h>
#include <OleAuto.h>

namespace ShelfManager::Infrastructure::Com {

class IRawQueuePriorityCheckApi {
public:
    virtual ~IRawQueuePriorityCheckApi() = default;

    // ベンダー関数comQueuePriorityCheckApiのBSTR入出力を隔離するRaw境界。
    // inputの所有権は呼出し側に残り、この関数は解放しない。
    // outputの正式な割当・解放責任はベンダーヘッダー受領後に確定する。
    // 現行AdapterはoutputをSysFreeStringで解放する契約として閉じ込める。
    //
    // THREAD: 実COM実装は専用STA Executor上から呼び出す。
    // SAFETY: 正式宣言または所有権契約が異なる場合は、この境界だけを
    // 差し替え、Application／DomainへBSTRを露出させない。
    // SOURCE: docs/superpowers/specs/2026-07-24-queue-priority-check-design-addendum.md。
    virtual HRESULT Check(BSTR input, BSTR* output) = 0;
};

}  // namespace ShelfManager::Infrastructure::Com
