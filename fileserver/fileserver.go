package main
 
import (
	"net/http"
)
 
func main() {
	http.Handle("/", http.FileServer(http.Dir("/var/tmp/www")))
	http.ListenAndServe(":80", nil)
}
