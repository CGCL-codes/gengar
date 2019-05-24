/**
 * <p>
 * Memory Usage
 * </p>
 *
 * @author Liu Cong
 * @since 2019/2/28
 */
var MemUsage = function () {

    // echarts theme
    var theme = "macarons";

    // Memory Usage Map:  Id -> MemUsage Object
    var memoryUsageMap = [];

    var $table = $('#MemUsageTable');

    var tableHeight = 430;

    var host = window.location.host;
    var gateOneUrl = "https://202.114.10.172:6037/";

    function initTable() {
        $table.bootstrapTable({
            uniqueId: 'id',
            toolbar: '#toolbar',
            search: true,
            showRefresh: true,
            showFullscreen: true,
            showColumns: true,
            showPaginationSwitch: true,
            pagination: true,
            // height: tableHeight,
            columns: [
                [{
                    title: 'ID',
                    field: 'id',
                    rowspan: 2,
                    align: 'center',
                    valign: 'middle',
                    formatter: idFormatter
                }, {
                    title: 'DRAM',
                    colspan: 4,
                    align: 'center'
                }, {
                    title: 'NVM',
                    colspan: 4,
                    align: 'center'
                }, {
                    title: 'Item Actions',
                    field: 'actions',
                    rowspan: 2,
                    align: 'center',
                    valign: 'middle',
                    events: actionsEvents,
                    formatter: actionsFormatter
                }],
                [{
                    title: 'Used',
                    field: 'dramUsed',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Unused',
                    field: 'dramUnused',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Total',
                    field: 'dramTotal',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Status',
                    field: 'dramStatus',
                    align: 'center',
                    valign: 'middle',
                    formatter: statusFormatter
                }, {
                    title: 'Used',
                    field: 'nvmUsed',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Unused',
                    field: 'nvmUnused',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Total',
                    field: 'nvmTotal',
                    align: 'center',
                    valign: 'middle',
                    formatter: memoryFormatter
                }, {
                    title: 'Status',
                    field: 'nvmStatus',
                    align: 'center',
                    valign: 'middle',
                    formatter: statusFormatter
                }]
            ]
        });
    }

    // 格式化id
    function idFormatter(value, row, index, field) {
        var data = row['data'];
        return [
            '<p class="text-primary">' + data["id"] + '</p>'
        ].join('');
    }

    // 处理内存单位，使其具有良好的可读性
    function memoryFormatter(value, row, index, field) {
        var data = row['data'];
        var inconInnerHtml;
        if (field === 'dramTotal' || field === 'nvmTotal')
            inconInnerHtml = "";
        else if (row['operation'] === 'insert') {
            if (field === 'dramUsed')
                inconInnerHtml = getTdIconInnerHtml(0, data[field], data["dramTotal"]);
            else if (field === 'nvmUsed')
                inconInnerHtml = getTdIconInnerHtml(0, data[field], data["nvmTotal"]);
            else if (field === 'dramUnused')
                inconInnerHtml = getTdIconInnerHtml(data["dramTotal"], data[field], data["dramTotal"]);
            else if (field === 'nvmUnused')
                inconInnerHtml = getTdIconInnerHtml(data["nvmTotal"], data[field], data["nvmTotal"]);
        }
        else if (row['operation'] === 'update') {
            if (field === 'dramUsed' || field === 'dramUnused')
                inconInnerHtml = getTdIconInnerHtml(row['preData'][field], data[field], data["dramTotal"]);
            else if (field === 'nvmUsed' || field === 'nvmUnused')
                inconInnerHtml = getTdIconInnerHtml(row['preData'][field], data[field], data["nvmTotal"]);
        }
        return [
            '<div class="d-flex flex-row">' +
            '<p class="text-info font-italic">' +
            unitConversion(data[field]) +
            '</p>' + inconInnerHtml +
            '</div>'
        ].join('');
    }

    // 格式化进度条
    function statusFormatter(value, row, index, field) {
        var data = row['data'];
        var kind = field === 'dramStatus' ? 'dram' : 'nvm';
        return getStatusHtml(data, kind);
    }

    // actions
    function actionsFormatter(value, row, index, field) {
        return [
            '<div class="btn-group">' +
            '  <button type="button" class="btn btn-secondary btn-sm dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">' +
            '    Action' +
            '  </button>' +
            '  <div class="dropdown-menu">' +
            '    <a name="showApps" class="dropdown-item" href="#">ShowApps</a>' +
            '  </div>' +
            '</div>'
        ].join('');
    }

    // actions 的事件
    var actionsEvents = {
        'click a[name="showApps"]': function (e, value, row, index) {
            var data = row['data'];
            addCard(data);
        }
    };

    /**
     * 添加或者更新table内容
     * @param param
     */
    function addOrUpdateTableRow(param) {
        var data = param.data;
        if (mapIsContainsKey(memoryUsageMap, data["id"])) {
            // 修改
            var preData = $table.bootstrapTable('getRowByUniqueId', data["id"])['data'];
            $table.bootstrapTable('updateByUniqueId', {
                id: data["id"],
                row: {
                    id: data["id"],
                    data: data,
                    preData: preData,
                    operation: 'update'
                }
            });
        } else {
            // 添加
            $table.bootstrapTable('insertRow', {
                index: getCountInMap(memoryUsageMap),
                row: {
                    id: data["id"],
                    data: data,
                    operation: 'insert'
                }
            });
            // 更新 web ssh 的高度
            // setWebShellDivSize();
        }
        // 更新缓存的数据
        memoryUsageMap[data["id"]] = data;
    }

    /**
     * 返回进度条指示
     * @param data
     * @param kind
     * @returns {string}
     */
    function getStatusHtml(data, kind) {
        var percent = (data[kind + "Total"] === 0 ? 0 : (data[kind + "Used"] * 100.0 / data[kind + "Total"]).toFixed(2));
        var width = percent + '%';
        var bgColor = 'bg-success';
        if (percent >= 80) bgColor = 'bg-danger';
        else if (percent >= 50) bgColor = 'bg-warning';
        return [
            '<div class="progress">' +
            '<div class="progress-bar progress-bar-striped progress-bar-animated ' + bgColor +
            '" role="progressbar" ' +
            'style="width: ' + width +
            '" aria-valuenow="' + percent +
            '" aria-valuemin="0" aria-valuemax="100">' + width + '</div>' +
            '</div>'
        ].join('');
    }

    /**
     * 根据prev和curr的起伏，设置相应的图标
     * @param prev
     * @param curr
     * @param total
     * @returns {string}
     */
    function getTdIconInnerHtml(prev, curr, total) {
        var icon, percent;
        if (prev < curr) {
            percent = (total === 0 ? 0 : ((curr - prev) * 100.0 / total).toFixed(2));
            icon = '<span class="oi oi-arrow-thick-top text-danger small">' + percent + '%</span>';
        } else if (prev > curr) {
            percent = (total === 0 ? 0 : ((prev - curr) * 100.0 / total).toFixed(2));
            icon = '<span class="oi oi-arrow-thick-bottom text-success small">' + percent + '%</span>';
        } else {
            icon = '<span class="oi oi-minus text-dark small">' + 0.00 + '%</span>';
        }
        return icon;
    }

    /**
     * 内存单位转换
     * @param num 初始单位为 B
     * @returns {string}
     */
    function unitConversion(num) {
        var unit = 'B';
        while (num >= 1024) {
            if (unit === 'B') unit = 'KB';
            else if (unit === 'KB') unit = 'MB';
            else if (unit === 'MB') unit = 'GB';
            else break;
            num /= 1024;
        }
        return num.toFixed(2) + ' ' + unit;
    }

    /**
     * 判断一个 map 中是否含有key
     * @param map
     * @param key
     * @returns {boolean}
     */
    function mapIsContainsKey(map, key) {
        for (var k in map) {
            if (k === key) {
                return true;
            }
        }
        return false;
    }

    /**
     * 返回字典内元素个数
     * @param map
     * @returns {number}
     */
    function getCountInMap(map) {
        var num = 0;
        for (var key in map) {
            num++;
        }
        return num;
    }

    /**
     * 处理消息
     * @param data
     */
    function handlerMsg(data) {
        // item : {"id":"server1","dramUsed":1000,"dramUnused":100,"dramTotal":1100,"nvmUsed":1000,"nvmUnused":200,"nvmTotal":1200}
        $.each(data, function (index, item) {
            addOrUpdateTableRow({data: item});
            updateCard(item);
        });
    }

    /**
     * 初始化 WebSocket
     */
    function initWebSocket() {
        var ws;
        if ('WebSocket' in window) {
            ws = new WebSocket("ws://" + host + "/ws")
        } else {
            ws = new SockJS("http://" + host + "/sockjs/ws");
        }

        //连接打开事件
        ws.onopen = function () {
            updateWebSocketAlert('alert alert-success small', 'Connection success !');
        };
        //收到消息事件
        ws.onmessage = function (msg) {
            // 将数据转成json对象
            var json = eval("(" + event.data + ")");
            console.log(json);
            // 如果信息出错，则直接返回，不作处理
            if (json["code"] !== 100) return;
            handlerMsg(json["extend"]["data"]);
        };
        //连接关闭事件
        ws.onclose = function () {
            updateWebSocketAlert('alert alert-dark small', 'Connection closed !');
        };
        //发生了错误事件
        ws.onerror = function () {
            updateWebSocketAlert('alert alert-danger small', 'Connection Error !');
        };

        //窗口关闭时，关闭连接
        window.unload = function () {
            ws.close();
        };
    }

    function updateWebSocketAlert(classAttr, info) {
        var $alert = $('#sysAlert');
        $alert.removeClass();
        $alert.addClass(classAttr);
        $alert.html('<strong>Websocket status : </strong> ' + info);
    }

    /**
     * 添加 Card
     * @param memoryInfo
     */
    function addCard(memoryInfo) {
        if ($('#' + memoryInfo["id"] + 'Card').length > 0) return;
        var wrap = $('#cardsContainer');
        var $card = $('<div id="' + memoryInfo["id"] + 'Card" class="card">\n' +
            '           <div class="card-header">' +
            '               <strong> ' + memoryInfo["id"] + ' </strong>' +
            '               <button name="close" type="button" class="ml-2 mb-1 close" aria-label="Close">\n' +
            '                    <span aria-hidden="true">&times;</span>\n' +
            '               </button>\n' +
            '           </div>\n' +
            '           <div class="card-body">\n' +
            '           </div>\n' +
            '          </div>');
        wrap.append($card);

        var pie = $('<div id="' + memoryInfo["id"] + 'Pie" style="width: 100%; height: 250px;"></div>');
        $card.find('.card-body').append(pie);
        // 初始化dran pie 和 nvm pie
        var pieInstance = echarts.init(document.getElementById(memoryInfo["id"] + 'Pie'), theme);
        var dataset = genData(memoryInfo);
        if (dataset.length === 1) {
            $card.find('strong').html(memoryInfo["id"] + " (no apps is running)");
        } else {
            $card.find('strong').html(memoryInfo["id"] + " (<span class=\"badge badge-success\">" + (dataset.length - 1) + "</span> apps is running)");
        }
        var option = {
            title: [{
                text: 'DRAM',
                left: '15%'
            }, {
                text: 'NVM',
                right: '20%'
            }],
            tooltip: {
                formatter: function (params) {
                    return [
                        params['name'],
                        '<br>',
                        unitConversion(params['value'][params['seriesIndex'] + 1]),
                        ' : ',
                        params['percent'] + '%'
                    ].join("");
                }
            },
            legend: {
                type: 'scroll',
                bottom: '0%'
            },
            dataset: {
                source: dataset
            },
            series: [
                {
                    name: 'DRAM',
                    type: 'pie',
                    radius: 60,
                    center: ['25%', '50%'],
                    itemStyle: {
                        emphasis: {
                            shadowBlur: 10,
                            shadowOffsetX: 0,
                            shadowColor: 'rgba(0, 0, 0, 0.5)'
                        }
                    },
                    encode: {
                        itemName: 'apps',
                        value: 'DRAM'
                    }
                }, {
                    name: 'NVM',
                    type: 'pie',
                    radius: 60,
                    center: ['75%', '50%'],
                    itemStyle: {
                        emphasis: {
                            shadowBlur: 10,
                            shadowOffsetX: 0,
                            shadowColor: 'rgba(0, 0, 0, 0.5)'
                        }
                    },
                    encode: {
                        itemName: 'apps',
                        value: 'NVM'
                    }
                }
            ]
        };
        pieInstance.setOption(option);
        // onclick
        $card.find('button[name="close"]').on('click', function () {
            removeCard(memoryInfo['id'] + 'Card');
        });
    }

    /**
     * 删除 card
     * @param domId
     */
    function removeCard(domId) {
        $('#' + domId).remove();
    }

    /**
     * 更新 card
     * @param memoryInfo
     */
    function updateCard(memoryInfo) {
        var $card = $('#' + memoryInfo["id"] + 'Card');
        if ($card.length <= 0) return;
        var pieInstance = echarts.getInstanceByDom(document.getElementById(memoryInfo["id"] + 'Pie'));
        var dataset = genData(memoryInfo);
        if (dataset.length === 1) {
            $card.find('strong').html(memoryInfo["id"] + " (no apps is running)");
        } else {
            $card.find('strong').html(memoryInfo["id"] + " (<span class=\"badge badge-success\">" + (dataset.length - 1) + "</span> apps is running)");
        }
        pieInstance.setOption({
            dataset: {
                source: dataset
            }
        });
    }

    /**
     * 把 memoryInfo 转化成 echart dataset 数据结构
     * @param memoryInfo
     * @returns {Array}
     */
    function genData(memoryInfo) {
        var source = [];
        source.push(['apps', 'DRAM', 'NVM']);
        var apps = memoryInfo['apps'];
        for (var i = 0; i < apps.length; i++) {
            var app = apps[i];
            var temp = [];
            temp.push(app['name']);
            temp.push(app['dramUsed']);
            temp.push(app['nvmUsed']);
            source.push(temp);
        }
        return source;
    }

    // 设置 $('#cardsContainer') 的高度为100%
    function setCardsContainerHeight() {
        var winHeight = $(window).height();
        var navHeight = $('nav').outerHeight();
        var alertHeight = $('.alert').outerHeight() + 16;
        $("#cardsContainer").height(winHeight - navHeight - alertHeight);
    }

    // 初始化 web ssh
    function initWebShell() {
        GateOne.init({
                url: gateOneUrl
            }
        );
    }

    // 设置 web ssh 的高度
    // function setWebShellDivSize() {
    //     var $gateoneContainer = $('#gateone_container');
    //     var winHeight = $(window).height();
    //     var navHeight = $('nav').outerHeight();
    //     var tableCurH = $gateoneContainer.prev().prev('.bootstrap-table').outerHeight();
    //     var tableHeightTmp = tableCurH < tableHeight ? tableCurH : tableHeight;
    //     var gateOneHeight = winHeight - navHeight - tableHeightTmp;
    //     $gateoneContainer.height(gateOneHeight < 300 ? 300 : gateOneHeight);
    // }

    // 当浏览器窗口大小变化时，设置 web ssh 和 cardsContainer 的高度
    function onWindowResize() {
        $(window).resize(function () {
            // setWebShellDivSize();
            setCardsContainerHeight();
        });
    }

    return {
        init: function () {
            initTable();
            initWebSocket(); // 初始化 web socket
            setCardsContainerHeight();
            // setWebShellDivSize();
            onWindowResize();
            // initWebShell();
        }
    }
}();


jQuery(document).ready(function () {
    MemUsage.init();
});
