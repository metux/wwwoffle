# By default, wwwoffle doesn't use /var/log/wwwoffle.log
# /var/log/wwwoffle.log {
# 	missingok
# 	rotate 1
# 	daily
# 	compress
# 	postrotate
#               if which invoke-rc.d >/dev/null 2>&1; then
#                   invoke-rc.d wwwoffle restart
#               else
#                   /etc/init.d/wwwoffle restart
#               fi >/dev/null
# 	endscript
# }

/var/log/wwwoffle-htdig.log {
	missingok
	rotate 1
	daily
	compress
}
