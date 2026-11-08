import QtQuick
import "BrandTokens.js" as Tokens

QtObject {
    id: theme
    property bool dark: true
    readonly property var tokens: dark ? Tokens.themes.dark : Tokens.themes.light
    readonly property var colors: tokens.colors
    readonly property var spacing: tokens.metrics.spacing
    readonly property var sizes: tokens.metrics.size
    readonly property var radius: tokens.metrics.radius
    readonly property var text: tokens.text
    readonly property int controlHeight: sizes["control-md"]
    readonly property string sans: sansRegular.name
    readonly property string sansMedium: sansMediumFont.name
    readonly property string sansSemibold: sansSemiboldFont.name
    readonly property string mono: monoRegular.name
    readonly property string monoMedium: monoMediumFont.name
    readonly property bool fontsReady: sansRegular.status === FontLoader.Ready
        && sansMediumFont.status === FontLoader.Ready && sansSemiboldFont.status === FontLoader.Ready
        && monoRegular.status === FontLoader.Ready && monoMediumFont.status === FontLoader.Ready
    readonly property FontLoader sansRegular: FontLoader { source: "qrc:/fonts/IBMPlexSans-Regular.ttf" }
    readonly property FontLoader sansMediumFont: FontLoader { source: "qrc:/fonts/IBMPlexSans-Medium.ttf" }
    readonly property FontLoader sansSemiboldFont: FontLoader { source: "qrc:/fonts/IBMPlexSans-SemiBold.ttf" }
    readonly property FontLoader monoRegular: FontLoader { source: "qrc:/fonts/IBMPlexMono-Regular.ttf" }
    readonly property FontLoader monoMediumFont: FontLoader { source: "qrc:/fonts/IBMPlexMono-Medium.ttf" }
}
