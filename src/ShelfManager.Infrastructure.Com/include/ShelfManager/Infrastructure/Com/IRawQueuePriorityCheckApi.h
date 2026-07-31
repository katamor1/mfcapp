#pragma once

#include <Windows.h>
#include <OleAuto.h>

namespace ShelfManager::Infrastructure::Com {

// ベンダー関数のABIをHRESULT／BSTRのまま隔離する最下層Port。
// BSTRは埋込みNULを含み得るため、実装と呼出し側はnull終端ではなくSysStringLenで扱う。
// 期待可能なCOM／通信失敗はHRESULTで返し、例外をApplication側へ通常経路として漏らさない。
// 正式契約が確定するまでは再入可能性や並行呼出し安全性を仮定せず、専用STA上で直列化する。
class IRawQueuePriorityCheckApi {
public:
    virtual ~IRawQueuePriorityCheckApi() = default;

    // ベンダー関数comQueuePriorityCheckApiのBSTR入出力を隔離するRaw境界。
    // inputの所有権は呼出し側に残り、この関数は解放しない。
    // outputの正式な割当・解放責任はベンダーヘッダー受領後に確定する。
    // 現行Adapterはoutput slotをnullで渡し、返された非null BSTRをHRESULTの成否に
    // かかわらずSysFreeStringで一度だけ解放する契約として閉じ込める。
    //
    // SUCCEEDEDはRaw呼出しの完了だけを示し、outputの非null、UTF-16／JSON妥当性、
    // 工具集合や使用時間の意味的整合を保証しない。上位Gatewayがすべて再検証する。
    // timeout、取消、冪等性、外部副作用の有無は正式ベンダー契約が確定するまで未保証である。
    //
    // THREAD: 実COM実装は専用STA Executor上から呼び出す。
    // SAFETY: 正式宣言または所有権契約が異なる場合は、この境界だけを
    // 差し替え、Application／DomainへBSTRを露出させない。
    // SOURCE: docs/superpowers/specs/2026-07-24-queue-priority-check-design-addendum.md。
    virtual HRESULT Check(BSTR input, BSTR* output) = 0;
};

}  // namespace ShelfManager::Infrastructure::Com
