$(function() {
	$('.menu .searchnavi .icon.closeicon').on('click', function() {
		$('.menu .searchnavi .searchfield').val('');
	});

	$('.uploadwindow').on('mouseenter', function() {
		$('.uploadwrap').addClass('active');
	});

	$('.uploadwindow').on('mouseleave', function() {
		$('.uploadwrap').removeClass('active');
	});
});