#pragma once

#include <string>

namespace ShelfManager::Presentation {

// 機械状態帯で使用するPresentation専用のLamp表現。
// Domainの列挙値と一対一ではなく、Presenterがオペレーター向けに集約する。
// enumの順序・整数値をResource ID、永続値、通信値として使用せず、Viewが表示へ明示変換する。
enum class StatusLampState {
    Unknown,
    Normal,
    Warning,
    Error,
    Disconnected
};

// 機械状態帯を一回描画するための完成済みViewModel。
// ViewはDomain状態を再解釈せず、この値に従ってテキスト、Lamp、操作可否を全体更新する。
// 各文字列はオペレーター表示専用であり、Domain値、ErrorCode、再試行条件へ解析し直さない。
// 機械状態帯は監視表示であり、ここから認証、順位変更、搬送等の外部操作を発火させない。
struct MachineStatusViewModel final {
    std::wstring connectionText;
    std::wstring machineText;
    std::wstring freshnessText;
    std::wstring machineModelText;
    std::wstring operationAvailabilityText;

    // 優先順位付けされた代表メッセージ。全診断の一覧、安定Error ID、監査記録ではない。
    std::wstring messageText;

    // connectionLampは通信生存状態、machineLampは機械Error／Warningを中心とする表示であり、
    // 運転モード、認証、データ鮮度、各機能の最終操作可否を単独では表さない。
    StatusLampState connectionLamp{StatusLampState::Unknown};
    StatusLampState machineLamp{StatusLampState::Unknown};

    // SAFETY: 機種SessionがResolvedでProfile実体を持つ場合だけtrueとなる。
    // 通信・鮮度・機械Error、認証、運転モード、対象別Policyは含まない。
    bool safetyOperationsEnabled{false};

    // SAFETY: Presenterが機種確定、Connected、Fresh、機械Errorなしを
    // すべて確認した場合だけtrueとなる。View側でtrueへ上書きしてはならない。
    // 認証、手動運転、搬送先可用性等の機能固有条件は各Presenter／Use Caseが追加確認する。
    bool controlsEnabled{false};

    // 初回MachineSnapshot未取得中であることを示す。機種Sessionだけは確定済みの場合がある。
    // falseは最新性や通信成功を保証せず、freshnessTextとcontrolsEnabledを併せて判断する。
    bool synchronizing{true};

    friend bool operator==(
        const MachineStatusViewModel& left,
        const MachineStatusViewModel& right) {
        return left.connectionText == right.connectionText &&
               left.machineText == right.machineText &&
               left.freshnessText == right.freshnessText &&
               left.machineModelText == right.machineModelText &&
               left.operationAvailabilityText ==
                   right.operationAvailabilityText &&
               left.messageText == right.messageText &&
               left.connectionLamp == right.connectionLamp &&
               left.machineLamp == right.machineLamp &&
               left.safetyOperationsEnabled ==
                   right.safetyOperationsEnabled &&
               left.controlsEnabled == right.controlsEnabled &&
               left.synchronizing == right.synchronizing;
    }

    friend bool operator!=(
        const MachineStatusViewModel& left,
        const MachineStatusViewModel& right) {
        return !(left == right);
    }
};

}  // namespace ShelfManager::Presentation
