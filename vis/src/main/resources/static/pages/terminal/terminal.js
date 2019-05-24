/**
 * <p>
 * Terminal
 * </p>
 *
 * @author Liu Cong
 * @since 2019/3/15
 */
var Terminal = function () {

    function initWebShell() {
        GateOne.init(
            {
                url: "https://202.114.10.172:6037/"
            }
        );
    }

    function setWebShellDivSize() {
        var winHeight = $(window).height();
        var navHeight = $('#nav').outerHeight();
        $("#gateone_container").height(winHeight - navHeight);
    }

    function onWindowResize() {
        $(window).resize(function () {
            setWebShellDivSize();
        });
    }

    return {
        init: function () {
            setWebShellDivSize();
            onWindowResize();
            initWebShell();
        }
    }
}();

jQuery(document).ready(function () {
    Terminal.init();
});
