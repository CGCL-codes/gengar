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

    // Memory Usage Id List
    var memoryUsageIdList = [];

    // init Memory usage Pie
    function initMemUsageGauge() {
        for (var i = 0; i < 5; i++) {
            var nodeName = "node" + i;
            addMemUsageGauge(nodeName, 50);
            memoryUsageIdList.push(nodeName + "MemUsage");
        }
    }

    /**
     * Add memory usage gauge
     * @param nodeName
     * @param value
     */
    function addMemUsageGauge(nodeName, value) {
        var card = $('<div class="card"></div>');
        var cardHeader = $('<h5 class="card-header">' + nodeName + '</h5>');
        card.append(cardHeader);
        var cardBody = $('<div class="card-body text-info"></div>');
        var row = $('<div class="row"></div>');
        var leftCol = $('<div class="col-lg-12 col-xl-4"></div>');
        var rightCol = $('<div class="col-lg-12 col-xl-8"></div>');
        leftCol.append('<p class="card-text">已使用内存大小：<strong>0 G</strong></p>');
        leftCol.append('<p class="card-text">未使用内存大小：<strong>0 G</strong></p>');
        leftCol.append('<p class="card-text">总共的内存大小：<strong>0 G</strong></p>');
        rightCol.append('<div id="' + nodeName + 'MemUsageGauge" style="height: 400px; width: 100%"></div>');
        leftCol.appendTo(row);
        rightCol.appendTo(row);
        row.appendTo(cardBody);
        cardBody.appendTo(card);
        card.appendTo($("#CardColumns"));
        var gauge = echarts.init(document.getElementById(nodeName + 'MemUsageGauge'), theme);
        var option = {
            tooltip: {
                formatter: "{b} <br/>{a} : {c}%"
            },
            series: [
                {
                    name: "内存使用率",
                    type: 'gauge',
                    detail: {formatter: '{value}%'},
                    data: [{value: value, name: nodeName}]
                }
            ]
        };
        gauge.setOption(option);
        setInterval(function () {
            option.series[0].data[0].value = (Math.random() * 100).toFixed(2) - 0;
            gauge.setOption(option, true);
        }, 2000);
    }

    /**
     * update the value of memory usage gauge
     * @param id
     * @param value
     */
    function updateMemUsageGauge(id, value, param) {
        // 更新 card text
        var pArr = $('#id').parent('.card-body').find('strong');
        // pArr.eq(0); // 已使用内存大小
        // pArr.eq(1); // 未使用内存大小
        // pArr.eq(2); // 总共内存大小
        // 更新 gauge
        var instance = echarts.getInstanceByDom(document.getElementById("id"));
        var option = instance.getOption();
        option.series[0].data[0].value = value;
        instance.setOption(option, true);
    }

    return {
        init: function () {
            initMemUsageGauge();
        }
    }
}();


jQuery(document).ready(function () {
    MemUsage.init();
});
