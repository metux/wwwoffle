//------------------------------------------------//
//                                                //
// WWWOFFLE Version 2.9 - Javascript functions    //
//                                                //
//------------------------------------------------//

//--------------------------//
// Index Page Form handling //
//--------------------------//

// Check all of the checkboxes named 'url' on the specified form.

function form_make_url_checked(form)
{
 for(i=0;i<form.url.length;i++)
   {
    form.url[i].checked=true;
   }
}

// Uncheck all of the checkboxes named 'url' on the specified form.

function form_make_url_unchecked(form)
{
 for(i=0;i<form.url.length;i++)
   {
    form.url[i].checked=false;
   }
}

// Toggle all of the checkboxes named 'url' on the specified form.

function form_toggle_url_checked(form)
{
 for(i=0;i<form.url.length;i++)
   {
    form.url[i].checked=!form.url[i].checked;
   }
}
