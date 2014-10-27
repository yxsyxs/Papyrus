/*
    Copyright (C) 2014 Sialan Labs
    http://labs.sialan.org

    Kaqaz is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Kaqaz is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.0

PaperAbstractButton {
    id: pw_btn
    background: "files/weather/weather-unknown.png"

    property string paperWeather: paperItem? database.paperWeather(paperItem) : ""
    property variant weatherChooser

    onClicked: {
        if( !weatherChooser )
            weatherChooser = showBottomPanel(paper_weather_component,true)
    }

    onRefresh: {
        background = "files/weather/" + paperWeather + ".png"
    }

    onPaperWeatherChanged: refresh()

    Component {
        id: paper_weather_component

        Column {
            width: 20

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 10*physicalPlatformScale
                font.family: SApp.globalFontFamily
                font.pixelSize: 12*fontsScale
                color: "#333333"
                text: qsTr("Please select weather")
            }

            ListView {
                id: pw_list
                height: 92*physicalPlatformScale
                width: parent.width
                orientation: ListView.Horizontal
                model: ListModel{}
                delegate: Item {
                    width: height
                    height: pw_list.height

                    Image {
                        anchors.fill: parent
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                        sourceSize: Qt.size(width,height)
                        source: "files/weather/" + iconId + ".png"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            paperObject.save()
                            database.setPaperWeather(paperItem,iconId)
                            pw_btn.refresh()
                            hideBottomPanel()
                        }
                    }
                }

                Component.onCompleted: {
                    pw_list.model.append( {"iconId": "weather-clear"} )
                    pw_list.model.append( {"iconId": "weather-clouds"} )
                    pw_list.model.append( {"iconId": "weather-overcast"} )
                    pw_list.model.append( {"iconId": "weather-showers"} )
                    pw_list.model.append( {"iconId": "weather-storm"} )
                    pw_list.model.append( {"iconId": "weather-snow"} )
                    pw_list.model.append( {"iconId": "weather-fog"} )
                    pw_list.model.append( {"iconId": "weather-unknown"} )
                }
            }
        }
    }
}
