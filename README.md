# hw1-FykasAthanasios SYSPRO_1 2025

## Προγράμματα
-fss_manager
-fss_console
-worker
-fss_script.sh

## Τι δεν έχει υλοποιηθεί
    Οι εντολές listAll, listMonitored, listStopped. Όλα τα υπόλοιπα ερωτήματα, μαζί και με την εντολή purge και έλεγχο flags/εισόδου στο fss_script έχουν υλοποιηθεί.

## Compilation
    Υπάρχει Makefile με separate compilation, υπάρχουν ξεχωριστοί κανόνες για το κάθε πρόγραμμα άμα κάποιος το επιθυμεί. Η εφαρμογή κάνει compile με την εντολή make, αντίστοιχα make clean για καθαρισμό των .ο αρχείων.

## Running the Program / Screen Prints
    Για τα prints της εφαρμογής και το κάλεσμα των προγραμμάτων χρησιμοποιήθηκαν τα standards
της εκφώνησης.

## Ροή του προγράμματος
    Θεωρείτε ότι το πρόγραμμα console δεν μπορεί να τρέξει εφόσον δεν τρέχει πίσω ο manager.

### fss_manager
    Καλώντας ./fss_manager -l <manager_logfile> -c <config_file> -n <worker_limit> τρέχουμε το πρόγραμμα.

    Υπάρχουν 3 βασικές δομές:
    -Worker* workers , απλή linked list με σκοπό την υλοποίηση του mapping worker pid - pipe, και για να έχω ανα πάσα στιγμή max - worker_limit workers active.

    -WatchNode *watch_head, απλή linked list με σκοπό την αποθήκευση των inotify watch (wd), καθώς και για αποθήκευση πληροφοριών όπως errors για την status εντολή

    -Queuehead* queue, απλή ουρά με σκοπό την αποθήκευση νέων incoming task, το πρόγραμμα κάνει enqueue ανάλογα με την εντολή ή Inotify event, και dequeue για να φτιάξει νέους workers εφόσον έχω την δυνατότητα.

        
    Αφού κάνει setup τους signal_handler, named pipes (fss_in),
    (fss_out), και άλλες απαραίτητες δομές αρχίζει διαβάζει από το config file κάνοντας enqueue τα ζευγάρια source_dir -> target_dir. Μετά εισέρχεται σε ένα loop, που αρχίζει να φτιάχνει workers, με το που φτάσει το worker limit ή η ουρά είναι άδεια, σπάει και εισέρχεται σε δεύτερο Loop που μπορεί να διαβάσει events από inotify και εντολές απο fss_in. Το πρόγραμμα γνωρίζει άμα υπάρχει κάποιο event για διάβασμα με την χρήση της polle στος file descriptors του inotify pipe και του fss_in. Αρχικά τσεκάρει άμα έχουν τελειώσει κάποια παιδιά για να κάνει collect το exit code τους και να κανει parse/print το exec report. Μετά άμα έχω event διαβάζω απο το αντίστοιχo pipe και τέλος άμα η ουρά δεν είναι άδεια και δεν εχω worker_limit workers, φτιάξε και άλλους workers.

### worker exec program
    Καλώντας ./worker source_dir target_dir filename operation τρέχουμε το πρόγραμμα.

    -Το πρόγραμμα χρησιμοποιεί μία απλή linked list για την αποθήκευση των errors με σκοπό τον εμφανισμό τους μετά στο exec report.

    Απλή if-else if λογική για τον προσδιορισμό ποιας εντολής είναι να εκτελεστεί και πράτωντας αντίστοιχα.

### fss_console
    Καλώντας ./fss_console -l <console-logfile> τρέχουμε το πρόγραμμα.

    Το πρόγραμμα με σκοπό να προσδιορίσει άμα υπάρχει κάτι να διαβάσει είτε στο stdin/ fss_out χρησιμοποιεί την select για εντοπισμό event, έτσι ώστε να μην κολλάει στην read.

    Τα μυνήματα που κάνει print έρχονται έτοιμα απο τον fss_manager και απλά κάνει write ότι διάβασε, οπότε δεν υπάρχει parsing logic.

### fss_script.sh
    Καλώντας ./fss_script.sh -p <path> -c <command> τρέχουμε το script.

    Έχουν υλοποιηθεί η εντολή purge καθώς και σωστός έλεγχος της εισόδου για flags/ arguments.