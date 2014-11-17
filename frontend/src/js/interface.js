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

	function upload(file) {
		var xhr = new XMLHttpRequest();

		$('.uploadprogress').show();

		xhr.upload.addEventListener('progress', function(event) {
			console.log('progess', file.name, event.loaded, event.total);

			var perc = Math.round((event.loaded / event.total) * 100);

			if (event.loaded == event.total) {
				$('.uploadprogress').hide();
			}

			$('.uploadprogress .label.progress').html(perc + '%');

		});
		xhr.addEventListener('readystatechange', function(event) {
			console.log(
				'ready state', 
				file.name, 
				xhr.readyState, 
				xhr.readyState == 4 && xhr.status
		 	);
		});

		xhr.open('POST', '/upload', true);
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
				$('#content').html('<div class="loading"></div>');
			},

			success: function(data) {
				$('#content').html($(data).find('#content').html());
			}
		});
	}

	/*
	$('.video').on('click', function() {
		var vidid = $(this).data('vidid');
		console.log('test');
		loadContent('/video/', {'vidid': vidid})
	});
	*/
	$('input.searchfield').on('input', function() {
		if ($(this).val() == "") {
			loadContent('/', {})
		} else {
			$('.button.search').click();
		}
	});

	$('.button.searchscene').on('click', function() {
		loadContent('/searchScene/', {'vidid':'0', 'frame':'0'})
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

	$('.button.search').on('click', function() {
		loadContent('/search/', {'name':$('.searchbar .searchfield').val()})

		//window.location.href =  + $('.searchbar .searchfield').val();
	});

	$('.button.upload').on('click', function() {
		$('#uploadfile').click();
	});

	$('#uploadfile').change(function(event) {
		for(var i = 0; i < event.target.files.length; i += 1) {
			upload(event.target.files[i]); 
		}
	});

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

	$('.scene .thumbnail, .originvideo .thumbnail, .originvideo .meta').on('click', function() {
		var $this = $(this).parent(),

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
		$videoplayer.attr('poster', poster);

		$videoplayer.find('source').attr('src', videourl).attr('type', 'video/' + format);

		$videoplayer.load();

		$videoplayer.ready(function() {

			$videoplayerWrap.fadeIn(500);

			$videoplayerWrap.promise().done(function() {
				var video = $videoplayer[0];

				video.currentTime = parseFloat(time);
				video.play();

				$videoplayer.on('play', function (e) {
					$('.videoplayer-wrap .videoplayer .overlay .button.searchscene').hide();
				});

				$videoplayer.on('pause', function (e) {
					$('.videoplayer-wrap .videoplayer .overlay .button.searchscene').show();
				});

			});

		});
	});

	$('.videoplayer-wrap .videoplayer .overlay .button.close').on('click', function() {
		var $videoplayerWrap = $('.videoplayer-wrap');

		$videoplayerWrap.find('video')[0].pause();

		$videoplayerWrap.hide();
	});

});