function upload(file, searchable) {
	var xhr = new XMLHttpRequest();

	$('.uploadprogress').show();

	xhr.upload.addEventListener('progress', function(event) {
		console.log('progress', file.name, event.loaded, event.total);

		if (event.loaded == event.total) {
			//$('.uploadprogress').hide();
			$('.uploadprogress .label.progressmessage').html('Upload successfully completed.').css('width', '225px');
			$('.uploadprogress .button.abort').html('CLOSE');
		}

	});
	xhr.addEventListener('readystatechange', function(event) {
		console.log(
			'ready state', 
			file.name, 
			xhr.readyState, 
			xhr.readyState == 4 && xhr.status
	 	);
	});

	xhr.open('POST', '/upload?searchable=' + searchable, true);
	xhr.setRequestHeader('X-Filename', file.name);

	console.log('sending', file.name, file);
	xhr.send(file);
}

function loadContent(url, data) {
	$.ajax({
		url: url,
		data: data,
		type: 'GET',
		
		beforeSend: function() {
			$('#content').html('<div class="loading">Please wait...</div>');
		},

		success: function(data) {
			$('#content').html($(data).find('#content').html());
			$(document).ready(function() {
				unbindEvents();
				bindEvents();
			});
		}
	});
}

function unbindEvents() {
	$('.scene .meta').unbind('click');
	$('.button.searchscene').unbind('click');
	$('.scene .thumbnail, .originvideo .thumbnail, .originvideo .meta, .similarscene .thumbnail').unbind('click');
	$('.videoplayer-wrap .videoplayer .overlay .button.close').unbind('click');
}

function bindEvents() {
	$('.scene .meta').on('click', function() {
		var $this = $(this),
			$thumb = $this.parent().find('.thumbnail'),
			display = $thumb.css('display');

		if ($thumb.hasClass('active')) {
			$thumb.slideUp().removeClass('active');
		} else {
			$('.scene .thumbnail.active').slideUp().removeClass('active');

			var $img = $thumb.find('img');

			$img.attr('src', $img.data('src'));

			$img.ready(function() {
				$thumb.slideDown().addClass('active');
				$this.parent().css({
					'background-color': '#a0a0a0',
					'color': '#fff'
				});
			});
		}
	});

	$('.button.searchscene').on('click', function() {
		var $video = $('video'),
			// Replaced this line to fix a bug in the similar scene search. Not quite sure what it does though
			// TODO: Removed these comments if this is alright
			vidid = $video.data('vidid'),
			// Workaround
			//vidid = $video[0].dataset.vidid,
			second = $video[0].currentTime;
	
		$('.videoplayer-wrap .videoplayer .overlay .button.close').click();
		
		loadContent('/searchScene/', {'vidid':vidid, 'second':second});
	});

	$('.video .thumbnail').on('mouseenter', function() {
		$(this).find('.icon.deleteicon').show();
	});

	$('.video .thumbnail').on('mouseleave', function() {
		$(this).find('.icon.deleteicon').css('display', '');
	});


	$('.scene .thumbnail, .originvideo .thumbnail, .similarscene .thumbnail').on('click', function() {
		var $this = $(this).parent(),
			
			vidid = $this.data('vidid'),
			videourl = $this.data('url'),
			format = $this.data('extension'),
			time = $this.data('time'),

	
			scenecount = $this.find('.meta .label.scenecount').html(),

			title = $this.find('.meta .label.title').html(),
			poster = $this.find('.thumbnail img').attr('src'),

			$videoplayerWrap = $('.videoplayer-wrap'),
			$overlayLabel = $videoplayerWrap.find('.overlay .label.title'),
			$videoplayer = $videoplayerWrap.find('video');

		if (scenecount != undefined) {
			title += " " + scenecount;
		}

		$overlayLabel.html(title);
		$videoplayer.data('vidid', vidid);
		$videoplayer.attr('poster', poster);

		$videoplayer.find('source').attr('src', videourl).attr('type', 'video/' + format);

		video = $videoplayer[0];

		video.load();

		video.oncanplay = function() {
			$videoplayerWrap.fadeIn(500);

			$videoplayerWrap.promise().done(function() {
				video.currentTime = parseFloat(time);
				
				video.onseeked = function() {
					video.play();
					video.onseeked = undefined;
				}

				video.onplay = function() {
					$('.videoplayer-wrap .videoplayer .overlay .button.searchscene').hide();
				};

				video.onpause = function() {
					$('.videoplayer-wrap .videoplayer .overlay .button.searchscene').show();
				};
			});

			video.oncanplay = undefined;
		};
	});

	$('.videoplayer-wrap .videoplayer .overlay .button.close').on('click', function() {
		var $videoplayerWrap = $('.videoplayer-wrap');

		$videoplayerWrap.find('video')[0].pause();

		$videoplayerWrap.hide();
	});
}

$(function() {
	
	$(document).keypress(function(e) {
	    /* Event which is triggered, when the "Enter"-Key is pressed */
	    if(e.which == 13) {
			/* Check if the user is inside the searchfield */
			if ($('input.searchfield').is(':focus')) {
				/* Press the "Go!"-Button for the user */
				$('.button.search').click();
			}
		}
	});

	bindEvents();

	$('input.searchfield').on('input', function() {
		if ($(this).val() == "") {
			loadContent('/', {})
		} else {
			$('.button.search').click();
		}
	});

	$('.menu .searchnavi .icon.closeicon').on('click', function() {
		$('.menu .searchnavi .searchfield').val('');
	});

	$('.uploadwindow').on('mouseenter', function() {
		$('.upload-wrap').addClass('active');
	});

	$('.uploadwindow').on('mouseleave', function() {
		$('.upload-wrap').removeClass('active');
	});

	$('.submenu').on('click', function() {
		$('.submenu-wrap').addClass('active');
	});

	$('.submenu-wrap').on('mouseleave', function() {
		$('.submenu-wrap').removeClass('active');
	});

	$('.uploadwindow').on('mouseleave', function() {
		$('.submenu-wrap').removeClass('active');
	});

	$('.button.search').on('click', function() {
		loadContent('/search/', {'name':$('.searchbar .searchfield').val()})

		//window.location.href =  + $('.searchbar .searchfield').val();
	});

	$('.button.upload').on('click', function() {
		$('#uploadfile').click();
	});

	$('.button.source').on('click', function() {
		$('#uploadassrc').click();
	});

	$('.button.index').on('click', function() {
		alert('Building tree was started.')
		$.ajax({
			url: '/shadowTree',
			type: 'GET',
		});
	});

	$('#uploadfile').change(function(event) {
		for(var i = 0; i < event.target.files.length; i += 1) {
			upload(event.target.files[i], 1);
		}
	});

	$('#uploadassrc').change(function(event) {
		for(var i = 0; i < event.target.files.length; i += 1) {
			upload(event.target.files[i], 0);
		}
	});

	$('.uploadprogress .button.abort').on('click', function() {
		$('.uploadprogress').hide();
		location.reload();
	});

	$('#filtersrc').change(function() {
		$.ajax({
			url: '/toggleFilter',
			type: 'GET',
		});
	});

});
